/*
 * ipc_server.h - Despacho de mensajes shim <-> servidor (C2 / §6.1).
 *
 * Cada cliente es un team_id con su propia tabla de handles. Todas las
 * peticiones se validan contra ese team: un handle ajeno es EACCES.
 */
#ifndef RG_IPC_SERVER_H
#define RG_IPC_SERVER_H

#include <stdint.h>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "../common/rg_types.h"
#include "../common/rg_ipc.h"
#include "memory_manager.h"
#include "ring_manager.h"
#include "cs_verifier.h"
#include "asic/rg_asic.h"
#include "gcn/gpuvm.h"

struct rg_team_state {
	uint32_t vmid = 0;
	bool has_vmid = false;
	std::unique_ptr<GpuVm> vm;
};

class RgServer {
public:
	RgServer(uint64_t vram_size, uint64_t gart_size);

	/* Procesa un mensaje. Devuelve bytes escritos en reply (>=0) o error. */
	int32_t handle(uint32_t team, uint32_t msg, const void *payload,
		uint32_t payload_size, void *reply, uint32_t reply_size);

	/* Cliente muerto: libera BOs, VMID y mappings. */
	void drop_team(uint32_t team);

	MemoryManager &memory() { return fMemory; }
	RingManager &rings() { return fRings; }
	CsVerifier &verifier() { return fVerifier; }

	/* Politica activa: deny-all salvo que se instale la de test/produccion. */
	void set_policy(const rg_cs_policy &policy) { fVerifier = CsVerifier(policy); }

	/* Priorización de tráfico IPC para app_server frente a tareas batch (Punto 15) */
	bool is_high_priority_msg(uint32_t msg) const {
		return msg == RG_CS || msg == RG_BO_MMAP;
	}

private:
	int32_t handle_info(void *reply, uint32_t reply_size);
	int32_t handle_bo_create(uint32_t team, const void *payload,
		uint32_t payload_size, void *reply, uint32_t reply_size);
	int32_t handle_bo_reply(uint32_t team, uint32_t msg, const void *payload,
		uint32_t payload_size, void *reply, uint32_t reply_size);
	int32_t handle_cs(uint32_t team, const void *payload,
		uint32_t payload_size, void *reply, uint32_t reply_size);

	rg_team_state &team_state(uint32_t team);

	MemoryManager fMemory;
	RingManager fRings;
	CsVerifier fVerifier;
	VmidManager fVmids;
	std::map<uint32_t, rg_team_state> fTeams;
	std::mutex fTeamsLock;
	rg_asic_desc fAsic{};
};

#endif /* RG_IPC_SERVER_H */
