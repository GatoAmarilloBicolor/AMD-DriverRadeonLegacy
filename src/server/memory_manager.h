/*
 * memory_manager.h - Gestor de BOs VRAM/GTT (C2 / §6.3).
 * Fase 8: eviccion LRU de BOs idle de VRAM a GTT. Si aun no cabe, se
 * degrada a GTT. La copia real VRAM<->GTT necesita DMA del servidor
 * [HUMANO]; en el host el contenido vive en host_ptr y se conserva.
 */
#ifndef RG_MEMORY_MANAGER_H
#define RG_MEMORY_MANAGER_H

#include <stdint.h>
#include <mutex>
#include <map>
#include <set>
#include <vector>

#include "../common/rg_types.h"
#include "../common/rg_ipc.h"

struct rg_bo {
	uint32_t handle;
	uint32_t team_id;
	uint64_t size;
	uint64_t alignment;
	uint32_t domains;
	uint32_t tiling;
	uint32_t flags;
	uint32_t busy;
	uint64_t last_used;    /* reloj LRU (solo BOs VRAM) */

	rg_domain domain;
	uint64_t gpu_addr;     /* direccion que ve la GPU */
	uint64_t offset;       /* offset dentro del dominio (VRAM) o de la GTT */
	int32_t  area;         /* area_id mapeada (o -1) */
	uint8_t *host_ptr;     /* puntero CPU (simulado o mapeado) */
	void    *fence_slot;   /* direccion GTT para fences, si aplica */
};

class MemoryManager {
public:
	MemoryManager(uint64_t vram_size, uint64_t gart_size);

	/* Devuelve 0 en exito; handle/area/gpu_addr en *out. */
	int32_t create_bo(uint32_t team, const rg_bo_create &req, rg_bo_reply *out);
	int32_t map_bo(uint32_t team, uint32_t handle, uint32_t map_flags, void **ptr);
	int32_t wait_bo_idle(uint32_t team, uint32_t handle, uint32_t timeout_ms);
	int32_t set_tiling(uint32_t team, uint32_t handle, uint32_t tiling);
	int32_t get_tiling(uint32_t team, uint32_t handle, uint32_t *tiling);
	int32_t is_busy(uint32_t team, uint32_t handle, uint32_t *busy);
	int32_t close_bo(uint32_t team, uint32_t handle);
	void    release_team(uint32_t team);

	/* Consulta para el verificador de CS: solo si el BO es del cliente. */
	const rg_bo *lookup(uint32_t team, uint32_t handle) const;

	uint64_t vram_used() const { return fVramUsed; }
	uint64_t gart_used() const { return fGartUsed; }
	uint32_t bo_count() const { return (uint32_t)fBos.size(); }
	uint64_t evictions() const { return fEvictions; }

private:
	struct Range { uint64_t offset, size; };
	int32_t alloc_vram(uint64_t size, uint64_t align, uint64_t *offset);
	void free_vram(uint64_t offset, uint64_t size);
	int32_t alloc_gtt(uint64_t size, uint64_t align, uint64_t *offset);
	void free_gtt(uint64_t offset, uint64_t size);
	bool try_evict_one(void);
	int32_t alloc_vram_evicting(uint64_t size, uint64_t align, uint64_t *offset);
	rg_bo *find_owned(uint32_t team, uint32_t handle);
	const rg_bo *find_owned(uint32_t team, uint32_t handle) const;
	void destroy_bo(rg_bo &bo);

	uint64_t fVramSize, fGartSize;
	uint64_t fVramUsed = 0, fGartUsed = 0;
	uint32_t fNextHandle = 1;
	uint64_t fClock = 1;
	uint64_t fEvictions = 0;
	int32_t  fNextArea = 1;

	std::vector<Range> fVramFree;
	std::vector<Range> fGartFree;
	std::map<uint32_t, rg_bo> fBos;
	mutable std::mutex fLock;
};

#endif /* RG_MEMORY_MANAGER_H */
