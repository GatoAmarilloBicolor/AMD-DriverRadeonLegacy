/*
 * gpuvm.h - GPUVM por cliente (C9 / §7A.5, ADR-010).
 *
 * En GFX6+ el aislamiento lo da el hardware. Este componente modela el
 * espacio de direcciones por cliente: reserva de VA, mapeo bo_va, VMIDs con
 * LRU y traduccion VA->PA. En el host la tabla de paginas se representa con
 * intervalos; en hardware se programaria la jerarquia real de 4 niveles.
 */
#ifndef RG_GPUV_M_H
#define RG_GPUV_M_H

#include <stdint.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "../../common/rg_types.h"

enum rg_bo_va_state {
	RG_BO_VA_INVALID = 0,
	RG_BO_VA_EVICTED,
	RG_BO_VA_MOVED,
	RG_BO_VA_IDLE
};

struct rg_bo_va {
	uint32_t handle;
	uint64_t va;
	uint64_t pa;
	uint64_t size;
	uint32_t flags;   /* RG_VM_READ/WRITE/EXEC */
	rg_bo_va_state state;
};

enum {
	RG_VM_READ = 1u << 0,
	RG_VM_WRITE = 1u << 1,
	RG_VM_EXEC = 1u << 2
};

class GpuVm {
public:
	explicit GpuVm(uint64_t va_size = (1ull << 40));
	~GpuVm();

	GpuVm(const GpuVm &) = delete;
	GpuVm &operator=(const GpuVm &) = delete;

	/* Reserva un rango de VA (cursor de tablas). Devuelve 0 o error. */
	int32_t reserve_va(uint32_t handle, uint64_t size, uint64_t alignment,
		uint32_t flags, rg_bo_va *out);
	int32_t map(uint32_t handle, uint64_t va, uint64_t pa, uint64_t size,
		uint32_t flags);
	int32_t unmap(uint32_t handle);
	int32_t evict(uint32_t handle);
	int32_t move(uint32_t handle, uint64_t new_pa);
	int32_t translate(uint64_t va, uint32_t access, uint64_t *pa_out) const;

	uint64_t next_va() const { return fNextVa; }
	size_t mapping_count() const { return fMappings.size(); }

	/* Fallo de pagina simulado: devuelve el handle culpable o 0. */
	uint32_t fault_owner(uint64_t va) const;

private:
	struct Range { uint64_t start, end; };
	uint64_t fVaSize;
	uint64_t fNextVa = 0x10000;
	std::map<uint32_t, rg_bo_va> fMappings;             /* handle -> mapeo */
	std::vector<std::pair<uint64_t, uint32_t>> fRanges;  /* va -> handle */
	mutable std::mutex fLock;
};

/* Gestor de VMIDs (16 en GFX6/7, mas en adelante). */
class VmidManager {
public:
	explicit VmidManager(uint32_t count = 16);
	/* Asigna el VMID del team; si no hay, roba el LRU. */
	int32_t acquire(uint32_t team, uint32_t *vmid_out);
	void release(uint32_t team);
	uint32_t owner_of(uint32_t vmid) const;

private:
	struct Slot { uint32_t team; uint64_t last_use; bool used; };
	std::vector<Slot> fSlots;
	uint64_t fClock = 0;
	mutable std::mutex fLock;
};

#endif /* RG_GPUV_M_H */
