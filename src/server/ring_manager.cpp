/*
 * ring_manager.cpp - Implementacion del anillo y las fences.
 */
#include "ring_manager.h"
#include "radeon_hardening.h"
#include <chrono>

RingManager::RingManager() = default;

int32_t RingManager::submit(rg_ring_id ring, const uint32_t *dwords, uint32_t count,
	uint64_t *fence_out)
{
	if (ring >= RG_RING_COUNT || dwords == nullptr || count == 0)
		return RG_EINVAL;
	
	/* Mitigación punto 11 y 13: Aplicar barrera de memoria de hardware y exclusión atómica */
	rg_hardware_memory_barrier();

	std::lock_guard<std::mutex> guard(fLock);
	Ring &r = fRings[ring];
	Entry e;
	e.fence = r.next_fence++;
	e.dwords.assign(dwords, dwords + count);
	r.queue.push_back(std::move(e));
	r.stats.submitted++;
	if (fence_out != nullptr)
		*fence_out = r.queue.back().fence;
	
	rg_hardware_memory_barrier();
	fCv.notify_all();
	return RG_OK;
}

void RingManager::drain()
{
	std::lock_guard<std::mutex> guard(fLock);
	for (uint32_t i = 0; i < RG_RING_COUNT; i++) {
		Ring &r = fRings[i];
		while (!r.queue.empty()) {
			r.done_fence = r.queue.front().fence;
			r.queue.pop_front();
			r.stats.completed++;
		}
	}
	fCv.notify_all();
}

int32_t RingManager::wait_fence(uint64_t fence, uint32_t timeout_ms)
{
	std::unique_lock<std::mutex> lock(fLock);
	auto done = [&]() {
		for (uint32_t i = 0; i < RG_RING_COUNT; i++)
			if (fRings[i].done_fence >= fence)
				return true;
		return false;
	};

	if (done())
		return RG_OK;

	lock.unlock();
	drain();
	lock.lock();

	if (done())
		return RG_OK;

	bool ok = fCv.wait_for(lock, std::chrono::milliseconds(timeout_ms), done);
	if (ok)
		return RG_OK;

	/* Mitigación de cuelgues (Punto 14): Disparo automático de soft_reset si el timeout expira */
	for (uint32_t i = 0; i < RG_RING_COUNT; i++) {
		fRings[i].stats.timeouts++;
	}
	lock.unlock();
	soft_reset();
	return RG_EINVAL;
}

uint64_t RingManager::last_fence(rg_ring_id ring) const
{
	std::lock_guard<std::mutex> guard(fLock);
	if (ring >= RG_RING_COUNT)
		return 0;
	return fRings[ring].next_fence - 1;
}

uint32_t RingManager::pending(rg_ring_id ring) const
{
	std::lock_guard<std::mutex> guard(fLock);
	if (ring >= RG_RING_COUNT)
		return 0;
	return (uint32_t)fRings[ring].queue.size();
}

int32_t RingManager::soft_reset()
{
	std::lock_guard<std::mutex> guard(fLock);
	for (uint32_t i = 0; i < RG_RING_COUNT; i++) {
		Ring &r = fRings[i];
		/* Todo lo pendiente se considera perdido; la fence salta al ultimo
		 * valor encolado para no bloquear a los clientes para siempre. */
		r.done_fence = r.next_fence - 1;
		r.queue.clear();
		r.stats.resets++;
	}
	fCv.notify_all();
	return RG_OK;
}
