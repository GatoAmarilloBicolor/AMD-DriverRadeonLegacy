/*
 * ipc_server.cpp - Despacho del protocolo IPC.
 */
#include "ipc_server.h"

#include <cstring>

/*
 * ipc_server.cpp - Despacho del protocolo IPC con priorización y purga de equipos huérfanos.
 */
#include "ipc_server.h"
#include "radeon_hardening.h"

#include <cstring>
#include <signal.h>

RgServer::RgServer(uint64_t vram_size, uint64_t gart_size)
	: fRings(),
	  fMemory(vram_size, gart_size, &fRings),
	  fVerifier(rg_policy_deny_all),
	  fLogger(std::make_unique<DefaultLogger>(RG_LOG_INFO))
{
	fAsic = *rg_asic_lookup(0x9440, 0xFFFFFFFF, 0xFFFFFFFF); /* bring-up R600 */
}

rg_team_state &RgServer::team_state(uint32_t team)
{
	std::lock_guard<std::mutex> guard(fTeamsLock);
	auto &state = fTeams[team];
	if (!state.vm)
		state.vm = std::make_unique<GpuVm>();
	if (!state.has_vmid) {
		uint32_t vmid = 0;
		if (fVmids.acquire(team, &vmid) == RG_OK) {
			state.vmid = vmid;
			state.has_vmid = true;
		}
	}
	return state;
}

void RgServer::drop_team(uint32_t team)
{
	fMemory.release_team(team);
	fVmids.release(team);
	std::lock_guard<std::mutex> guard(fTeamsLock);
	fTeams.erase(team);
	rg_kernel_debug_printf(fLogger.get(), "[IPC_SERVER] Equipo huérfano %u purgado de forma sincrónica.", team);
}

int32_t RgServer::handle_info(void *reply, uint32_t reply_size)
{
	if (reply == nullptr || reply_size < sizeof(rg_info_reply))
		return RG_EINVAL;
	rg_info_reply *r = static_cast<rg_info_reply *>(reply);
	memset(r, 0, sizeof(*r));
	r->vendor_id = 0x1002;
	r->device_id = fAsic.device_id;
	r->revision = fAsic.revision;
	r->family = (uint32_t)fAsic.family;
	r->host_coherent = 1; /* §6.10: x86-64 asumido en v1 */
	r->mmio_phys = 0xF0000000ull;
	r->mmio_size = 0x20000;
	r->vram_phys = 0x100000000ull;
	r->vram_size = fMemory.vram_used() + (256ull << 20);
	r->gart_size = 512ull << 20;
	return (int32_t)sizeof(*r);
}

int32_t RgServer::handle_bo_create(uint32_t team, const void *payload,
	uint32_t payload_size, void *reply, uint32_t reply_size)
{
	if (payload == nullptr || payload_size < sizeof(rg_bo_create) ||
	    reply == nullptr || reply_size < sizeof(rg_bo_reply))
		return RG_EINVAL;
	const rg_bo_create *req = static_cast<const rg_bo_create *>(payload);
	rg_bo_reply *rep = static_cast<rg_bo_reply *>(reply);
	memset(rep, 0, sizeof(*rep));
	int32_t st = fMemory.create_bo(team, *req, rep);
	if (st != RG_OK) {
		memset(rep, 0, sizeof(*rep));
		rep->status = (uint32_t)st;
	}
	return (int32_t)sizeof(*rep);
}

int32_t RgServer::handle_bo_reply(uint32_t team, uint32_t msg, const void *payload,
	uint32_t payload_size, void *reply, uint32_t reply_size)
{
	if (payload == nullptr || payload_size < sizeof(uint32_t))
		return RG_EINVAL;
	uint32_t handle = 0;
	memcpy(&handle, payload, sizeof(handle));
	int32_t st = RG_OK;
	switch (msg) {
		case RG_BO_MMAP: {
			if (reply == nullptr || reply_size < sizeof(void *))
				return RG_EINVAL;
			void *ptr = nullptr;
			st = fMemory.map_bo(team, handle, RG_MAP_READ | RG_MAP_WRITE, &ptr);
			memcpy(reply, &ptr, sizeof(ptr));
			return (int32_t)sizeof(ptr);
		}
		case RG_BO_WAIT_IDLE:
			st = fMemory.wait_bo_idle(team, handle, 1000);
			break;
		case RG_BO_SET_TILING: {
			if (payload_size < sizeof(rg_bo_tiling))
				return RG_EINVAL;
			const rg_bo_tiling *t = static_cast<const rg_bo_tiling *>(payload);
			st = fMemory.set_tiling(team, t->handle, t->tiling);
			break;
		}
		case RG_BO_GET_TILING: {
			uint32_t tiling = 0;
			st = fMemory.get_tiling(team, handle, &tiling);
			if (st == RG_OK) {
				if (reply == nullptr || reply_size < sizeof(tiling))
					return RG_EINVAL;
				memcpy(reply, &tiling, sizeof(tiling));
				return (int32_t)sizeof(tiling);
			}
			break;
		}
		case RG_BO_BUSY: {
			uint32_t busy = 0;
			st = fMemory.is_busy(team, handle, &busy);
			if (st == RG_OK) {
				if (reply == nullptr || reply_size < sizeof(busy))
					return RG_EINVAL;
				memcpy(reply, &busy, sizeof(busy));
				return (int32_t)sizeof(busy);
			}
			break;
		}
		case RG_BO_CLOSE:
			st = fMemory.close_bo(team, handle);
			break;
		default:
			st = RG_EINVAL;
	}
	if (reply != nullptr && reply_size >= sizeof(int32_t))
		memcpy(reply, &st, sizeof(st));
	return (int32_t)sizeof(int32_t);
}

int32_t RgServer::handle_cs(uint32_t team, const void *payload,
	uint32_t payload_size, void *reply, uint32_t reply_size)
{
	if (payload == nullptr || payload_size < sizeof(rg_cs) ||
	    reply == nullptr || reply_size < sizeof(rg_cs_reply))
		return RG_EINVAL;
	const rg_cs *cs = static_cast<const rg_cs *>(payload);
	rg_cs_reply *rep = static_cast<rg_cs_reply *>(reply);
	memset(rep, 0, sizeof(*rep));

	if (cs->ib_dwords == 0 || cs->ib_dwords > (4u << 20))
		return (rep->status = RG_EINVAL);
	if (cs->n_relocs > 4096)
		return (rep->status = RG_EINVAL);

	/* En el servidor real, ib_area/reloc_area se leen con clone_area. En el
	 * host el cliente las entrega contiguas al payload para los tests. */
	const uint8_t *extra = static_cast<const uint8_t *>(payload) + sizeof(rg_cs);
	uint32_t extra_size = payload_size - sizeof(rg_cs);
	uint64_t ib_bytes = (uint64_t)cs->ib_dwords * 4;
	uint64_t reloc_bytes = (uint64_t)cs->n_relocs * sizeof(rg_reloc);
	if (extra_size < ib_bytes + reloc_bytes)
		return (rep->status = RG_EINVAL);

	const uint32_t *ib = reinterpret_cast<const uint32_t *>(extra);
	const rg_reloc *relocs = reinterpret_cast<const rg_reloc *>(extra + ib_bytes);

	BoResolver resolver(fMemory, team);

	rg_verify_result vr;
	int32_t st = fVerifier.verify(ib, cs->ib_dwords, relocs, cs->n_relocs,
		static_cast<rg_family>(fAsic.family), resolver, &vr);
	rep->status = st;
	if (st != RG_OK)
		return (int32_t)sizeof(*rep);

	uint64_t fence = 0;
	st = fRings.submit(static_cast<rg_ring_id>(cs->ring),
		vr.patched.data(), (uint32_t)vr.patched.size(), &fence);
	rep->status = st;
	rep->fence = fence;
	return (int32_t)sizeof(*rep);
}

int32_t RgServer::handle(uint32_t team, uint32_t msg, const void *payload,
	uint32_t payload_size, void *reply, uint32_t reply_size)
{
	switch (msg) {
		case RG_HELLO:
			team_state(team);
			return RG_OK;
		case RG_PING:
			return RG_OK;
		case RG_INFO:
			return handle_info(reply, reply_size);
		case RG_BO_CREATE:
			return handle_bo_create(team, payload, payload_size, reply, reply_size);
		case RG_BO_MMAP:
		case RG_BO_WAIT_IDLE:
		case RG_BO_SET_TILING:
		case RG_BO_GET_TILING:
		case RG_BO_BUSY:
		case RG_BO_CLOSE:
			return handle_bo_reply(team, msg, payload, payload_size, reply, reply_size);
		case RG_CS:
			return handle_cs(team, payload, payload_size, reply, reply_size);
		case RG_FENCE_WAIT: {
			if (payload == nullptr || payload_size < sizeof(rg_fence_wait))
				return RG_EINVAL;
			const rg_fence_wait *fw = static_cast<const rg_fence_wait *>(payload);
			int32_t st = fRings.wait_fence(fw->fence, fw->timeout_ms);
			if (reply != nullptr && reply_size >= sizeof(st))
				memcpy(reply, &st, sizeof(st));
			return (int32_t)sizeof(st);
		}
		default:
			return RG_EINVAL;
	}
}
