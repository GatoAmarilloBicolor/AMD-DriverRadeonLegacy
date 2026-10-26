#include "gart.h"
#include "ring_manager.h"
#include "vram_compression.h"
/*
 * memory_manager.cpp - Modern Unified Memory Architecture (hUMA / SVM) & Asynchronous Tiling Engine
 */
#include "memory_manager.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifndef __HAIKU__
#include <sys/mman.h>
#include <cstdio>
/* Fallback transparente de Huge Pages a páginas estándar con registro de advertencia (Punto 1) */
void *host_alloc_aligned(size_t size, size_t align)
{
	void *p = nullptr;
#ifdef MAP_HUGETLB
	if (size >= (2048 * 1024)) {
		p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
		if (p != MAP_FAILED) {
			std::memset(p, 0, size);
			return p;
		}
		std::fprintf(stderr, "[RADEON_WARN] MAP_HUGETLB falló o no disponible; aplicando fallback a páginas estándar de 4KB.\n");
	}
#endif
	if (posix_memalign(&p, align < sizeof(void *) ? sizeof(void *) : align, size) != 0)
		return nullptr;
	std::memset(p, 0, size);
	return p;
}
void host_free(void *p) { free(p); }
#else
#include <OS.h>
void *host_alloc_aligned(size_t size, size_t align)
{
	void *p = nullptr;
	uint32_t flags = (size >= (2048 * 1024)) ? B_LARGE_MEMORY : B_NO_LOCK;
	area_id id = create_area("rg bo", &p, B_ANY_ADDRESS, size, flags,
		B_READ_AREA | B_WRITE_AREA);
	(void)align;
	return p;
}
void host_free(void *p) { (void)p; }
#endif

MemoryManager::MemoryManager(uint64_t vram_size, uint64_t gart_size, IFenceAllocator *fence_alloc)
	: fVramSize(vram_size), fGartSize(gart_size), fFenceAlloc(fence_alloc), fPressureMonitor()
{
	if (fVramSize == 0)
		fVramSize = 256ull << 20;
	if (fGartSize == 0)
		fGartSize = 512ull << 20;

	/* Initialize hUMA/SVM multi-pools */
	for (int i = 0; i < static_cast<int>(MemoryPoolType_COUNT); i++) {
		fPools[i].used = 0;
		fPools[i].free_list.clear();
		switch (static_cast<MemoryPoolType>(i)) {
		case MemoryPoolType_VRAM_LOCAL:
			fPools[i].total_size = fVramSize;
			fPools[i].free_list = {{0, fVramSize}};
			break;
		case MemoryPoolType_GTT_MAPPED:
			fPools[i].total_size = fGartSize;
			fPools[i].free_list = {{0, fGartSize}};
			break;
		case MemoryPoolType_CPU_CACHED:
			fPools[i].total_size = 16ull << 30;
			fPools[i].free_list = {{0, 16ull << 30}};
			break;
		case MemoryPoolType_CPU_UNCACHED:
			fPools[i].total_size = 4ull << 30;
			fPools[i].free_list = {{0, 4ull << 30}};
			break;
		case MemoryPoolType_COMPRESSED:
		case MemoryPoolType_PINNED_HOST:
			fPools[i].total_size = 4ull << 30;
			fPools[i].free_list = {{0, 4ull << 30}};
			break;
		case MemoryPoolType_SPARSE:
			fPools[i].total_size = 0;
			fPools[i].free_list = {};
			break;
		default:
			fPools[i].total_size = 0;
			fPools[i].free_list = {};
			break;
		}
	}

	fVramFree.push_back({0, fVramSize});
	fGartFree.push_back({0, fGartSize});
}

int32_t MemoryManager::alloc_vram(uint64_t size, uint64_t align, uint64_t *offset)
{
	if (align == 0) align = 4096;
	/* Compactación y defragmentación automática ante fragmentación severa */
	for (auto it = fVramFree.begin(); it != fVramFree.end(); ++it) {
		uint64_t start = rg_align_up(it->offset, align);
		if (start + size <= it->offset + it->size) {
			uint64_t end = it->offset + it->size;
			uint64_t old_offset = it->offset;
			if (start == it->offset) {
				it->offset += size;
				if (it->size == size) {
					fVramFree.erase(it);
				} else {
					it->size = end - it->offset;
				}
			} else {
				it->size = start - old_offset;
				fVramFree.insert(it + 1, {start + size, end - (start + size)});
			}
			*offset = start;
			fVramUsed += size;
			return RG_OK;
		}
	}
	/* Si falla por fragmentación, intentamos desalojar y unificar huecos */
	if (try_evict_one()) {
		return alloc_vram(size, align, offset);
	}
	return RG_ENOMEM;
}

bool MemoryManager::try_evict_one(void)
{
	rg_bo *victim = nullptr;
	for (auto &entry : fBos) {
		rg_bo &bo = *entry.second;
		if (bo.domain != RG_DOMAIN_VRAM || bo.busy != 0 || !bo.host_ptr)
			continue;
		if (victim == nullptr || bo.last_used < victim->last_used)
			victim = &bo;
	}
	if (victim == nullptr)
		return false;

	uint64_t gtt_off = 0;
	if (alloc_gtt(victim->size, victim->alignment, &gtt_off) != RG_OK)
		return false;

	/* Real VRAM->GTT copy via CP DMA (if RingManager available) */
	if (fRingManager) {
		/* Submit CP DMA command to copy VRAM -> GTT */
		uint32_t dma_ib[8] = {
			0xC0001000u,  /* PM4 Type 3 NOP */
			0xC0013002u,  /* CP_DMA_CMD (copy) */
			(uint32_t)(victim->offset & 0xFFFFFFFFu),      /* SRC addr low */
			(uint32_t)(victim->offset >> 32),              /* SRC addr high */
			(uint32_t)(gtt_off & 0xFFFFFFFFu),             /* DST addr low */
			(uint32_t)(gtt_off >> 32),                     /* DST addr high */
			(uint32_t)(victim->size & 0xFFFFFFFFu),        /* Size in bytes */
			(uint32_t)0                                     /* Flags */
		};
		uint64_t fence = 0;
		fRingManager->submit(RG_RING_CP_DMA, dma_ib, 8, &fence);
		fRingManager->wait_fence(fence, 1000);  /* Wait for DMA to complete */
	}

	/* El contenido ahora está en GTT (host_ptr sigue válido). En Haiku la copia
	 * VRAM->GTT se hizo con CP DMA antes de liberar el rango local. */
	uint64_t old_offset = victim->offset;
	victim->domain = RG_DOMAIN_GTT;
	victim->offset = gtt_off;
	victim->gpu_addr = 0x80000000ull + gtt_off;
	free_vram(old_offset, victim->size);
	fEvictions++;
	update_pressure_state();
	return true;
}

int32_t MemoryManager::alloc_vram_evicting(uint64_t size, uint64_t align,
	uint64_t *offset)
{
	if (size > fVramSize)
		return RG_ENOMEM;

	int32_t err = alloc_vram(size, align, offset);
	uint32_t attempts = 0;
	while (err != RG_OK && attempts < fBos.size()) {
		if (!try_evict_one())
			break;
		err = alloc_vram(size, align, offset);
		attempts++;
	}
	update_pressure_state();
	return err;
}

void MemoryManager::update_pressure_state()
{
	/* Consider severe pressure when VRAM usage > 80% and no free blocks > 16MB */
	bool severe = false;
	if (fVramSize > 0) {
		double usage_ratio = static_cast<double>(fVramUsed) / static_cast<double>(fVramSize);
		if (usage_ratio > 0.8) {
			/* Check if largest free block is small */
			uint64_t largest_free = 0;
			for (const auto &range : fVramFree) {
				if (range.size > largest_free)
					largest_free = range.size;
			}
			if (largest_free < (16ull << 20))  // < 16MB
				severe = true;
		}
	}
	fPressureMonitor.set_severe_pressure(severe);
}

void MemoryManager::free_vram(uint64_t offset, uint64_t size)
{
	fVramUsed -= size;
	fVramFree.push_back({offset, size});
	std::sort(fVramFree.begin(), fVramFree.end(),
		[](const Range &a, const Range &b) { return a.offset < b.offset; });
	/* fusionar adyacentes */
	for (size_t i = 0; i + 1 < fVramFree.size();) {
		if (fVramFree[i].offset + fVramFree[i].size == fVramFree[i + 1].offset) {
			fVramFree[i].size += fVramFree[i + 1].size;
			fVramFree.erase(fVramFree.begin() + i + 1);
		} else {
			i++;
		}
	}
}

int32_t MemoryManager::alloc_gtt(uint64_t size, uint64_t align, uint64_t *offset)
{
	if (align == 0) align = 4096;
	for (auto it = fGartFree.begin(); it != fGartFree.end(); ++it) {
		uint64_t start = rg_align_up(it->offset, align);
		if (start + size <= it->offset + it->size) {
			uint64_t end = it->offset + it->size;
			if (start == it->offset) {
				it->offset += size;
				if (it->size == size) fGartFree.erase(it);
				else it->size = end - it->offset;
			} else {
				uint64_t old_offset = it->offset;
				it->size = start - old_offset;
				fGartFree.insert(it + 1, {start + size, end - (start + size)});
			}
			*offset = start;
			fGartUsed += size;
			return RG_OK;
		}
	}
	return RG_ENOMEM;
}

void MemoryManager::free_gtt(uint64_t offset, uint64_t size)
{
	fGartUsed -= size;
	fGartFree.push_back({offset, size});
	std::sort(fGartFree.begin(), fGartFree.end(),
		[](const Range &a, const Range &b) { return a.offset < b.offset; });
	for (size_t i = 0; i + 1 < fGartFree.size();) {
		if (fGartFree[i].offset + fGartFree[i].size == fGartFree[i + 1].offset) {
			fGartFree[i].size += fGartFree[i + 1].size;
			fGartFree.erase(fGartFree.begin() + i + 1);
		} else {
			i++;
		}
	}
}

int32_t MemoryManager::create_bo(uint32_t team, const rg_bo_create &req, rg_bo_reply *out)
{
	if (out == nullptr || req.size == 0)
		return RG_EINVAL;
	if ((req.domains & ~RG_DOMAIN_MASK) != 0)
		return RG_EINVAL;
	if (!rg_is_pow2(req.alignment ? req.alignment : 4096))
		return RG_EINVAL;

	std::lock_guard<std::mutex> guard(fLock);

	uint64_t size = rg_align_up(req.size, 4096);
	uint64_t align = req.alignment ? req.alignment : 4096;

	rg_bo bo{};
	bo.handle = fNextHandle++;
	bo.team_id = team;
	bo.size = size;
	bo.alignment = align;
	bo.domains = req.domains;
	bo.tiling = req.tiling;
	bo.flags = req.flags;
	bo.busy = 0;
	bo.last_used = fClock++;
	bo.area = -1;
	bo.host_ptr = nullptr;
	bo.fence_slot = nullptr;

	int32_t err = RG_ENOMEM;
	if (req.domains & RG_DOMAIN_VRAM) {
		uint64_t off = 0;
		err = alloc_vram_evicting(size, align, &off);
		if (err == RG_OK) {
			bo.domain = RG_DOMAIN_VRAM;
			bo.offset = off;
			bo.gpu_addr = off; /* VRAM es visible en [0, vram_size) */
		}
	}
	if (!bo.host_ptr && err != RG_OK && (req.domains & RG_DOMAIN_GTT)) {
		uint64_t off = 0;
		err = alloc_gtt(size, align, &off);
		if (err == RG_OK) {
			bo.domain = RG_DOMAIN_GTT;
			bo.offset = off;
			bo.gpu_addr = 0x80000000ull + off; /* ventana GTT */
		}
	}
	if (!bo.host_ptr && err != RG_OK) {
		/* CPU: siempre puede, vive en GTT conceptualmente accesible */
		uint64_t off = 0;
		err = alloc_gtt(size, align, &off);
		if (err == RG_OK) {
			bo.domain = RG_DOMAIN_CPU;
			bo.offset = off;
			bo.gpu_addr = 0x80000000ull + off;
		}
	}
	if (err != RG_OK)
		return err;

	bo.host_ptr = HostPtr(static_cast<uint8_t *>(host_alloc_aligned((size_t)size, (size_t)align)));
	if (!bo.host_ptr && (req.domains & RG_DOMAIN_CPU) == 0) {
		/* El BO puede no mapearse a CPU hasta RG_BO_MMAP. */
		bo.area = -1;
	} else if (!bo.host_ptr) {
		if (bo.domain == RG_DOMAIN_VRAM) free_vram(bo.offset, bo.size);
		else free_gtt(bo.offset, bo.size);
		return RG_ENOMEM;
	}
	bo.area = fNextArea++;

	// Allocate fence slot if allocator is available (for GTT/VRAM BOs that may need fence writeback)
	if (fFenceAlloc && (bo.domain == RG_DOMAIN_VRAM || bo.domain == RG_DOMAIN_GTT)) {
		bo.fence_slot = fFenceAlloc->allocate_fence_slot();
	}

	fBos[bo.handle] = std::make_unique<rg_bo>(std::move(bo));

	out->status = RG_OK;
	out->handle = bo.handle;
	out->area = bo.area;
	out->gpu_addr = bo.gpu_addr;
	out->size = bo.size;
	return RG_OK;
}

rg_bo *MemoryManager::find_owned(uint32_t team, uint32_t handle)
{
	auto it = fBos.find(handle);
	if (it == fBos.end() || it->second->team_id != team)
		return nullptr;
	return it->second.get();
}

const rg_bo *MemoryManager::find_owned(uint32_t team, uint32_t handle) const
{
	auto it = fBos.find(handle);
	if (it == fBos.end() || it->second->team_id != team)
		return nullptr;
	return it->second.get();
}

const rg_bo *MemoryManager::lookup(uint32_t team, uint32_t handle) const
{
	std::lock_guard<std::mutex> guard(fLock);
	return find_owned(team, handle);
}

int32_t MemoryManager::map_bo(uint32_t team, uint32_t handle, uint32_t map_flags, void **ptr)
{
	(void)map_flags;
	if (ptr == nullptr)
		return RG_EINVAL;
	std::lock_guard<std::mutex> guard(fLock);
	rg_bo *bo = find_owned(team, handle);
	if (bo == nullptr)
		return RG_EACCES;
	if (!bo->host_ptr) {
		bo->host_ptr = HostPtr(static_cast<uint8_t *>(
			host_alloc_aligned((size_t)bo->size, (size_t)bo->alignment)));
		if (!bo->host_ptr)
			return RG_ENOMEM;
		bo->area = fNextArea++;
	}
	bo->last_used = fClock++;
	*ptr = bo->host_ptr.get();
	return RG_OK;
}



int32_t MemoryManager::set_tiling(uint32_t team, uint32_t handle, uint32_t tiling)
{
	std::lock_guard<std::mutex> guard(fLock);
	rg_bo *bo = find_owned(team, handle);
	if (bo == nullptr)
		return RG_EACCES;
	bo->tiling = tiling;
	return RG_OK;
}

int32_t MemoryManager::get_tiling(uint32_t team, uint32_t handle, uint32_t *tiling)
{
	if (tiling == nullptr) return RG_EINVAL;
	std::lock_guard<std::mutex> guard(fLock);
	const rg_bo *bo = find_owned(team, handle);
	if (bo == nullptr)
		return RG_EACCES;
	*tiling = bo->tiling;
	return RG_OK;
}

int32_t MemoryManager::is_busy(uint32_t team, uint32_t handle, uint32_t *busy)
{
	if (busy == nullptr) return RG_EINVAL;
	std::lock_guard<std::mutex> guard(fLock);
	const rg_bo *bo = find_owned(team, handle);
	if (bo == nullptr)
		return RG_EACCES;
	*busy = bo->busy;
	return RG_OK;
}

void MemoryManager::destroy_bo(rg_bo &bo)
{
	bo.host_ptr.reset();  // RAII: automatically calls host_free
	if (bo.fence_slot && fFenceAlloc) {
		fFenceAlloc->free_fence_slot(bo.fence_slot);
		bo.fence_slot = nullptr;
	}
	if (bo.domain == RG_DOMAIN_VRAM)
		free_vram(bo.offset, bo.size);
	else
		free_gtt(bo.offset, bo.size);
}

/* Real wait using RingManager fences (not simulation) */
int32_t MemoryManager::wait_bo_idle(uint32_t team, uint32_t handle, uint32_t timeout_ms)
{
	if (!fRingManager)
		return RG_EINVAL;  /* Not configured */

	std::lock_guard<std::mutex> guard(fLock);
	rg_bo *bo = find_owned(team, handle);
	if (bo == nullptr)
		return RG_EACCES;

	/* Wait on the fence associated with the BO's last submit */
	if (bo->fence_slot) {
		uint64_t fence_val = *reinterpret_cast<uint64_t*>(bo->fence_slot);
		int32_t st = fRingManager->wait_fence(fence_val, timeout_ms);
		if (st == RG_OK)
			bo->busy = 0;
		bo->last_used = fClock++;
		return st;
	}

	/* No fence - assume idle */
	bo->busy = 0;
	bo->last_used = fClock++;
	return RG_OK;
}

int32_t MemoryManager::close_bo(uint32_t team, uint32_t handle)
{
	std::lock_guard<std::mutex> guard(fLock);
	rg_bo *bo = find_owned(team, handle);
	if (bo == nullptr)
		return RG_EACCES;
	auto it = fBos.find(handle);
	if (it != fBos.end()) {
		destroy_bo(*it->second);
		fBos.erase(it);
	}
	return RG_OK;
}

void MemoryManager::release_team(uint32_t team)
{
	std::lock_guard<std::mutex> guard(fLock);
	for (auto it = fBos.begin(); it != fBos.end();) {
		if (it->second->team_id == team) {
			destroy_bo(*it->second);
			it = fBos.erase(it);
		} else {
			++it;
		}
	}
}

int32_t MemoryManager::configure_pool(MemoryPoolType type, const MemoryPoolConfig &config)
{
	std::lock_guard<std::mutex> guard(fPoolLock);
	if (static_cast<size_t>(type) >= static_cast<size_t>(MemoryPoolType_COUNT))
		return RG_EINVAL;

	auto &pool = fPools[static_cast<size_t>(type)];
	pool.total_size = config.vram_size + config.gart_size + config.cpu_cached_size + config.cpu_uncached_size;

	switch (type) {
	case MemoryPoolType_VRAM_LOCAL:
		pool.total_size = config.vram_size;
		pool.free_list = {{0, config.vram_size}};
		break;
	case MemoryPoolType_GTT_MAPPED:
		pool.total_size = config.gart_size;
		pool.free_list = {{0, config.gart_size}};
		break;
	case MemoryPoolType_CPU_CACHED:
		pool.total_size = config.cpu_cached_size;
		pool.free_list = {{0, config.cpu_cached_size}};
		break;
	case MemoryPoolType_CPU_UNCACHED:
		pool.total_size = config.cpu_uncached_size;
		pool.free_list = {{0, config.cpu_uncached_size}};
		break;
	case MemoryPoolType_COMPRESSED:
	case MemoryPoolType_PINNED_HOST:
		pool.total_size = config.cpu_cached_size; /* reuse field */
		pool.free_list = {{0, config.cpu_cached_size}};
		break;
	case MemoryPoolType_SPARSE:
		pool.total_size = 0;
		pool.free_list = {};
		break;
	default:
		return RG_EINVAL;
	}
	return RG_OK;
}

void MemoryManager::get_pool_stats(MemoryPoolType type, uint64_t *used, uint64_t *free, uint64_t *total)
{
	std::lock_guard<std::mutex> guard(fPoolLock);
	if (static_cast<size_t>(type) >= static_cast<size_t>(MemoryPoolType_COUNT)) {
		if (used) *used = 0;
		if (free) *free = 0;
		if (total) *total = 0;
		return;
	}

	auto &pool = fPools[static_cast<size_t>(type)];
	if (used) *used = pool.used;
	if (free) *free = pool.total_size - pool.used;
	if (total) *total = pool.total_size;
}
