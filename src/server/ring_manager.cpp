/*
 * ring_manager.cpp - Implementacion del anillo y las fences.
 */
#include "ring_manager.h"
#include "radeon_hardening.h"
#include <chrono>
#include <cstdlib>

#define ROCM_LOG_INFO 1
#define ROCM_LOG_DEBUG 0

#define ROCM_LOG(level, fmt, ...) \
    do { fprintf(stderr, "[RING %s] " fmt "\n", \
        (level == ROCM_LOG_INFO ? "INFO" : "DEBUG"), ##__VA_ARGS__); } while(0)

RingManager::RingManager() = default;

void *RingManager::allocate_fence_slot()
{
	std::lock_guard<std::mutex> guard(fFenceLock);
	for (auto &slot : fFenceSlots) {
		if (!slot.in_use) {
			slot.in_use = true;
			return slot.cpu_addr;
		}
	}
	// Allocate new slot (host simulation: 8 bytes for uint64_t fence)
	void *cpu_addr = std::malloc(8);
	if (!cpu_addr)
		return nullptr;
	uint64_t gpu_addr = reinterpret_cast<uintptr_t>(cpu_addr); // Simulated
	fFenceSlots.push_back({cpu_addr, gpu_addr, true});
	return cpu_addr;
}

void RingManager::free_fence_slot(void *slot)
{
	if (!slot)
		return;
	std::lock_guard<std::mutex> guard(fFenceLock);
	for (auto &s : fFenceSlots) {
		if (s.cpu_addr == slot) {
			s.in_use = false;
			break;
		}
	}
}

uint64_t RingManager::slot_gpu_addr(void *slot) const
{
	std::lock_guard<std::mutex> guard(fFenceLock);
	for (const auto &s : fFenceSlots) {
		if (s.cpu_addr == slot)
			return s.gpu_addr;
	}
	return 0;
}

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

int32_t RingManager::wait_fence(uint64_t fence, uint32_t timeout_ms)
{
	if (fence == 0)
		return RG_OK;
	
	std::unique_lock<std::mutex> lock(fLock);
	auto done = [this, fence]() {
		for (uint32_t i = 0; i < RG_RING_COUNT; i++) {
			/* Skip IH ring for fence waiting - it doesn't use fences */
			if (i == RG_RING_IH)
				continue;
			/* Skip rings that have never had work submitted (next_fence == 1) */
			if (fRings[i].next_fence == 1)
				continue;
			if (fRings[i].done_fence < fence)
				return false;
		}
		return true;
	};
	
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
		if (i == RG_RING_IH)
			continue;
		Ring &r = fRings[i];
		/* Todo lo pendiente se considera perdido; la fence salta al ultimo
		 * valor encolado para no bloquear los clientes para siempre. */
		r.done_fence = r.next_fence - 1;
		r.queue.clear();
		r.stats.resets++;
	}
	fCv.notify_all();
	return RG_OK;
}

/* Simulacion: procesa todo lo pendiente (lo haria la GPU). */
void RingManager::drain()
{
	std::lock_guard<std::mutex> guard(fLock);
	for (uint32_t i = 0; i < RG_RING_COUNT; i++) {
		if (i == RG_RING_IH)
			continue;
		Ring &r = fRings[i];
		r.done_fence = r.next_fence - 1;
		r.stats.completed += r.queue.size();
		r.queue.clear();
	}
	fCv.notify_all();
}

/* =====================================================================
 * IH (Interrupt Handler) Ring Management
 * ===================================================================== */

int32_t RingManager::ih_init(uint64_t gpu_addr, uint32_t size_dwords)
{
    if (size_dwords == 0 || (size_dwords & (size_dwords - 1)) != 0)
        return RG_EINVAL;  /* Size must be power of 2 */

    std::lock_guard<std::mutex> guard(fLock);
    
    /* Initialize IH ring buffer */
    Ring &ih = fRings[RG_RING_IH];
    ih.queue.clear();
    ih.next_fence = 1;
    ih.done_fence = 0;
    
    /* Store IH ring parameters */
    ih.gpu_addr = gpu_addr;
    ih.size_dwords = size_dwords;
    ih.rptr = 0;
    ih.wptr = 0;
    
    ROCM_LOG(ROCM_LOG_INFO, "IH ring initialized: gpu_addr=0x%llX, size=%u dwords",
             gpu_addr, size_dwords);
    
    return RG_OK;
}

int32_t RingManager::ih_fini()
{
    std::lock_guard<std::mutex> guard(fLock);
    
    Ring &ih = fRings[RG_RING_IH];
    ih.queue.clear();
    ih.next_fence = 1;
    ih.done_fence = 0;
    ih.gpu_addr = 0;
    ih.size_dwords = 0;
    ih.rptr = 0;
    ih.wptr = 0;
    
    return RG_OK;
}

uint32_t RingManager::ih_process(uint32_t max_entries)
{
    std::lock_guard<std::mutex> guard(fLock);
    
    Ring &ih = fRings[RG_RING_IH];
    if (ih.size_dwords == 0)
        return 0;
    
    /* In real hardware, we would read from the IH ring buffer at ih.gpu_addr
     * and process interrupt entries. Here we simulate processing. */
    
    uint32_t processed = 0;
    while (!ih.queue.empty() && processed < max_entries) {
        ih.queue.pop_front();
        processed++;
    }
    
    /* Update RPTR */
    ih.rptr = (ih.rptr + processed) % ih.size_dwords;
    
    return processed;
}

uint64_t RingManager::ih_get_rptr()
{
    std::lock_guard<std::mutex> guard(fLock);
    return fRings[RG_RING_IH].rptr;
}

void RingManager::ih_set_rptr(uint32_t rptr)
{
    std::lock_guard<std::mutex> guard(fLock);
    fRings[RG_RING_IH].rptr = rptr;
}

uint64_t RingManager::ih_get_wptr()
{
    std::lock_guard<std::mutex> guard(fLock);
    Ring &ih = fRings[RG_RING_IH];
    return ((uint64_t)ih.wptr_hi << 32) | ih.wptr_lo;
}

void RingManager::ih_set_wptr(uint64_t wptr)
{
    std::lock_guard<std::mutex> guard(fLock);
    Ring &ih = fRings[RG_RING_IH];
    ih.wptr_lo = (uint32_t)wptr;
    ih.wptr_hi = (uint32_t)(wptr >> 32);
}

uint32_t RingManager::ih_get_wptr_lo()
{
    std::lock_guard<std::mutex> guard(fLock);
    return fRings[RG_RING_IH].wptr_lo;
}

uint32_t RingManager::ih_get_wptr_hi()
{
    std::lock_guard<std::mutex> guard(fLock);
    return fRings[RG_RING_IH].wptr_hi;
}