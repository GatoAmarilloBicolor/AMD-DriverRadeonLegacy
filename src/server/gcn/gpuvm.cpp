/*
 * gpuvm.cpp - Implementacion de GPUVM y VMIDs.
 */
#include "gpuvm.h"

#include <algorithm>
#include <cstdint>

GpuVm::GpuVm(uint64_t va_size)
	: fVaSize(va_size ? va_size : (1ull << 40))
{
}

GpuVm::~GpuVm() = default;

int32_t GpuVm::reserve_va(uint32_t handle, uint64_t size, uint64_t alignment,
	uint32_t flags, rg_bo_va *out)
{
	if (out == nullptr || size == 0)
		return RG_EINVAL;
	if (alignment == 0)
		alignment = 4096;
	if (!rg_is_pow2(alignment))
		return RG_EINVAL;

	std::lock_guard<std::mutex> guard(fLock);
	if (fMappings.count(handle) != 0)
		return RG_EBUSY;

	uint64_t va = rg_align_up(fNextVa, alignment);
	uint64_t end = rg_align_up(va + size, 4096);
	if (va >= fVaSize || end > fVaSize || end < va)
		return RG_ENOMEM;

	rg_bo_va m{};
	m.handle = handle;
	m.va = va;
	m.pa = 0;
	m.size = size;
	m.flags = flags;
	m.state = RG_BO_VA_IDLE;
	fMappings[handle] = m;
	fRanges.emplace_back(va, handle);
	std::sort(fRanges.begin(), fRanges.end());
	fNextVa = end;
	*out = m;
	return RG_OK;
}

int32_t GpuVm::map(uint32_t handle, uint64_t va, uint64_t pa, uint64_t size,
	uint32_t flags)
{
	if (size == 0)
		return RG_EINVAL;
	std::lock_guard<std::mutex> guard(fLock);
	auto it = fMappings.find(handle);
	if (it == fMappings.end())
		return RG_ENOENT;
	if (it->second.size < size)
		return RG_EINVAL;
	/* Solapamiento con otro mapeo */
	for (const auto &kv : fMappings) {
		if (kv.first == handle)
			continue;
		if (va < kv.second.va + kv.second.size && kv.second.va < va + size)
			return RG_EBUSY;
	}
	it->second.va = va;
	it->second.pa = pa;
	it->second.flags = flags;
	it->second.state = RG_BO_VA_IDLE;
	fRanges.erase(std::remove_if(fRanges.begin(), fRanges.end(),
		[&](const std::pair<uint64_t, uint32_t> &p) { return p.second == handle; }),
		fRanges.end());
	fRanges.emplace_back(va, handle);
	std::sort(fRanges.begin(), fRanges.end());
	return RG_OK;
}

int32_t GpuVm::unmap(uint32_t handle)
{
	std::lock_guard<std::mutex> guard(fLock);
	auto it = fMappings.find(handle);
	if (it == fMappings.end())
		return RG_ENOENT;
	fRanges.erase(std::remove_if(fRanges.begin(), fRanges.end(),
		[&](const std::pair<uint64_t, uint32_t> &p) { return p.second == handle; }),
		fRanges.end());
	fMappings.erase(it);
	return RG_OK;
}

int32_t GpuVm::evict(uint32_t handle)
{
	std::lock_guard<std::mutex> guard(fLock);
	auto it = fMappings.find(handle);
	if (it == fMappings.end())
		return RG_ENOENT;
	it->second.state = RG_BO_VA_EVICTED;
	it->second.pa = 0;
	return RG_OK;
}

int32_t GpuVm::move(uint32_t handle, uint64_t new_pa)
{
	std::lock_guard<std::mutex> guard(fLock);
	auto it = fMappings.find(handle);
	if (it == fMappings.end())
		return RG_ENOENT;
	it->second.pa = new_pa;
	it->second.state = RG_BO_VA_MOVED;
	return RG_OK;
}

int32_t GpuVm::translate(uint64_t va, uint32_t access, uint64_t *pa_out) const
{
	if (pa_out == nullptr)
		return RG_EINVAL;
	std::lock_guard<std::mutex> guard(fLock);
	for (const auto &kv : fMappings) {
		const rg_bo_va &m = kv.second;
		if (m.state != RG_BO_VA_IDLE)
			continue;
		if (va >= m.va && va < m.va + m.size) {
			if ((access & RG_VM_WRITE) && !(m.flags & RG_VM_WRITE))
				return RG_EPERM;
			if ((access & RG_VM_READ) && !(m.flags & RG_VM_READ))
				return RG_EPERM;
			if ((access & RG_VM_EXEC) && !(m.flags & RG_VM_EXEC))
				return RG_EPERM;
			*pa_out = m.pa + (va - m.va);
			return RG_OK;
		}
	}
	return RG_EACCES; /* fallo de pagina / acceso invalido */
}

uint32_t GpuVm::fault_owner(uint64_t va) const
{
	std::lock_guard<std::mutex> guard(fLock);
	for (const auto &kv : fMappings) {
		const rg_bo_va &m = kv.second;
		if (va >= m.va && va < m.va + m.size)
			return m.handle;
	}
	return 0;
}

VmidManager::VmidManager(uint32_t count)
{
	if (count == 0)
		count = 16;
	fSlots.resize(count);
	for (auto &s : fSlots)
		s = {0, 0, false};
}

int32_t VmidManager::acquire(uint32_t team, uint32_t *vmid_out)
{
	if (vmid_out == nullptr)
		return RG_EINVAL;
	std::lock_guard<std::mutex> guard(fLock);
	for (uint32_t i = 0; i < fSlots.size(); i++) {
		if (fSlots[i].used && fSlots[i].team == team) {
			fSlots[i].last_use = ++fClock;
			*vmid_out = i;
			return RG_OK;
		}
	}
	for (uint32_t i = 0; i < fSlots.size(); i++) {
		if (!fSlots[i].used) {
			fSlots[i].used = true;
			fSlots[i].team = team;
			fSlots[i].last_use = ++fClock;
			*vmid_out = i;
			return RG_OK;
		}
	}
	/* LRU: robar el mas antiguo */
	uint32_t victim = 0;
	uint64_t oldest = UINT64_MAX;
	for (uint32_t i = 0; i < fSlots.size(); i++) {
		if (fSlots[i].last_use < oldest) {
			oldest = fSlots[i].last_use;
			victim = i;
		}
	}
	fSlots[victim].team = team;
	fSlots[victim].last_use = ++fClock;
	*vmid_out = victim;
	return RG_OK;
}

void VmidManager::release(uint32_t team)
{
	std::lock_guard<std::mutex> guard(fLock);
	for (auto &s : fSlots) {
		if (s.used && s.team == team)
			s = {0, 0, false};
	}
}

uint32_t VmidManager::owner_of(uint32_t vmid) const
{
	std::lock_guard<std::mutex> guard(fLock);
	if (vmid >= fSlots.size() || !fSlots[vmid].used)
		return 0;
	return fSlots[vmid].team;
}
