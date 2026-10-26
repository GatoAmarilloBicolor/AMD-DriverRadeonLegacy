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

/* Interface for fence slot allocation (decouples MemoryManager from RingManager) */
class IFenceAllocator {
public:
    virtual ~IFenceAllocator() = default;
    virtual void *allocate_fence_slot() = 0;   // Returns GTT CPU address
    virtual void free_fence_slot(void *slot) = 0;
    virtual uint64_t slot_gpu_addr(void *slot) const = 0;
};

enum rg_ring_id {
	RG_RING_GFX = 0,
	RG_RING_CP_DMA = 1,
	RG_RING_IH = 2,        /* Interrupt Handler ring */
	RG_RING_COUNT = 3
};

struct rg_ring_stats {
	uint64_t submitted;
	uint64_t completed;
	uint64_t timeouts;
	uint64_t resets;
};

class RingManager : public IFenceAllocator {
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

	// IFenceAllocator implementation
	void *allocate_fence_slot() override;
	void free_fence_slot(void *slot) override;
	uint64_t slot_gpu_addr(void *slot) const override;

	/* IH (Interrupt Handler) ring management */
	int32_t ih_init(uint64_t gpu_addr, uint32_t size_dwords);
	int32_t ih_fini();
	uint32_t ih_process(uint32_t max_entries);
	uint64_t ih_get_rptr();
	void ih_set_rptr(uint32_t rptr);
	uint64_t ih_get_wptr();
	void ih_set_wptr(uint64_t wptr);
	uint32_t ih_get_wptr_lo();
	uint32_t ih_get_wptr_hi();

	static constexpr int32_t kTimeout = -110;

private:
	struct Entry { uint64_t fence; std::vector<uint32_t> dwords; };
	struct Ring {
		std::deque<Entry> queue;
		uint64_t next_fence = 1;
		uint64_t done_fence = 0;
		rg_ring_stats stats{};
		
		/* IH (Interrupt Handler) specific fields */
		uint64_t gpu_addr = 0;
		uint32_t size_dwords = 0;
		uint32_t rptr = 0;
		uint32_t wptr = 0;
		uint32_t wptr_lo = 0;
		uint32_t wptr_hi = 0;
	};
	
	Ring fRings[RG_RING_COUNT];
	mutable std::mutex fLock;
	std::condition_variable fCv;
	
	// Fence slot pool (host simulation: just malloc)
	struct FenceSlot {
		void *cpu_addr;
		uint64_t gpu_addr;
		bool in_use = false;
	};
	std::vector<FenceSlot> fFenceSlots;
	mutable std::mutex fFenceLock;
};

#endif /* RG_RING_MANAGER_H */