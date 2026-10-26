/*
 * gart.cpp - Modelo de la tabla GART (Fase 2).
 */
#include "gart.h"

#include <cstring>

static const uint64_t kPageSize = 4096;
static const uint64_t kEntryValid = 1ull << 63;

GartTable::GartTable(uint64_t size, uint64_t gpu_base)
	: fSize(size), fGpuBase(gpu_base)
{
	fSize = rg_align_up(fSize, kPageSize);
	/* Una entrada por pagina; 0 significa "no mapeada". */
	fEntries.assign((size_t)(fSize / kPageSize), 0);
}

uint32_t GartTable::page_index(uint64_t offset) const
{
	return (uint32_t)(offset / kPageSize);
}

uint32_t GartTable::mapped_pages() const
{
	std::lock_guard<std::mutex> guard(fLock);
	uint32_t n = 0;
	for (uint64_t e : fEntries)
		if (e & kEntryValid)
			n++;
	return n;
}

int32_t GartTable::write_pages(uint64_t offset, uint64_t phys, uint64_t size)
{
	if ((offset % kPageSize) != 0 || (phys % kPageSize) != 0
		|| (size % kPageSize) != 0)
		return RG_EINVAL;

	uint64_t pages = size / kPageSize;
	uint32_t first = page_index(offset);
	if (first + pages > fEntries.size())
		return RG_EINVAL;

	for (uint64_t i = 0; i < pages; i++)
		fEntries[first + i] = (phys + i * kPageSize) | kEntryValid;
	return RG_OK;
}

int32_t GartTable::map(uint64_t offset, const rg_phys_range *ranges,
	uint32_t n_ranges)
{
	if (ranges == nullptr && n_ranges != 0)
		return RG_EINVAL;
	if (offset % kPageSize != 0)
		return RG_EINVAL;

	uint64_t cursor = offset;
	for (uint32_t i = 0; i < n_ranges; i++) {
		if ((ranges[i].size % kPageSize) != 0)
			return RG_EINVAL;
		int32_t err = write_pages(cursor, ranges[i].phys, ranges[i].size);
		if (err != RG_OK)
			return err;
		cursor += ranges[i].size;
	}
	invalidate();
	return RG_OK;
}

int32_t GartTable::unmap(uint64_t offset, uint64_t size)
{
	if ((offset % kPageSize) != 0 || (size % kPageSize) != 0)
		return RG_EINVAL;

	uint64_t pages = size / kPageSize;
	uint32_t first = page_index(offset);
	if (first + pages > fEntries.size())
		return RG_EINVAL;
	std::memset(fEntries.data() + first, 0, (size_t)pages * sizeof(uint64_t));
	invalidate();
	return RG_OK;
}

int32_t GartTable::translate(uint64_t gpu_addr, uint64_t *phys) const
{
	if (phys == nullptr)
		return RG_EINVAL;
	if (gpu_addr < fGpuBase)
		return RG_EACCES;
	uint64_t offset = gpu_addr - fGpuBase;
	if (offset >= fSize)
		return RG_EACCES;

	std::lock_guard<std::mutex> guard(fLock);
	uint64_t entry = fEntries[page_index(offset)];
	if ((entry & kEntryValid) == 0)
		return RG_ENOENT;
	*phys = (entry & ~kEntryValid) + (offset % kPageSize);
	return RG_OK;
}

/* Programa la tabla GART en hardware via PM4 writes a los registros GART.
 * En host simula; en Haiku escribe los registros GART via RingManager. */
int32_t GartTable::program_hardware(class RingManager *ring, uint64_t gart_base_phys)
{
	(void)gart_base_phys;

	if (!ring)
		return RG_EINVAL;

	/* En hardware real:
	 * 1. Deshabilitar GART (GART_ENABLE = 0)
	 * 2. Escribir GART_TABLE_BASE (direccion fisica de la tabla de paginas)
	 * 3. Escribir GART_CACHE_CNTL (invalidar cache)
	 * 4. Habilitar GART (GART_ENABLE = 1)
	 * 5. Flush TLB
	 *
	 * En host solo invalidamos la cache simulada.
	 */
	invalidate();

	/* Simulacion: solo marcamos que se programo */
	return RG_OK;
}