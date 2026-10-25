/*
 * ring_manager.h - Anillo CP, fences e IRQ (C2 / §6.5).
 *
 * Modelo de fence por writeback: el servidor reserva un slot en GTT, la GPU
 * escribe el valor al terminar, y el servidor compara. En el host se simula
 * de forma sincrona pero la API es la definitiva.
 */
#ifndef RG_RING_MANAGER_H
#define RG_RING_MANAGER_H

#include <stdint.h>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>

#include "../common/rg_types.h"

enum rg_ring_id {
	RG_RING_GFX = 0,
	RG_RING_CP_DMA = 1,
	RG_RING_COUNT = 2
};

struct rg_ring_stats {
	uint64_t submitted;
	uint64_t completed;
	uint64_t timeouts;
	uint64_t resets;
};

class RingManager {
public:
	RingManager();

	/* Encola un IB (ya verificado y parcheado). Devuelve la fence. */
	int32_t submit(rg_ring_id ring, const uint32_t *dwords, uint32_t count,
		uint64_t *fence_out);
	/* Espera a que la fence alcance el valor. 0 = ok, RG_ETIMEDOUT virtual. */
	int32_t wait_fence(uint64_t fence, uint32_t timeout_ms);
	uint64_t last_fence(rg_ring_id ring) const;
	uint32_t pending(rg_ring_id ring) const;

	/* Simulacion: procesa todo lo pendiente (lo haria la GPU). */
	void drain();

	/* GPU colgada: reset suave y contextos perdidos. */
	int32_t soft_reset();

	const rg_ring_stats &stats(rg_ring_id ring) const { return fRings[ring].stats; }

	static constexpr int32_t kTimeout = -110;

private:
	struct Entry { uint64_t fence; std::vector<uint32_t> dwords; };
	struct Ring {
		std::deque<Entry> queue;
		uint64_t next_fence = 1;
		uint64_t done_fence = 0;
		rg_ring_stats stats{};
	};
	Ring fRings[RG_RING_COUNT];
	mutable std::mutex fLock;
	std::condition_variable fCv;
};

#endif /* RG_RING_MANAGER_H */
