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

#include "memory_manager.h"
#include "ring_manager.h"
#include "cs_verifier.h"
#include "logger.h"
#include "asic/rg_asic.h"
#include "gcn/gpuvm.h"

/* Zero-allocation BO resolver for CsVerifier (implements IBoResolver) */
class BoResolver : public CsVerifier::IBoResolver {
public:
	BoResolver(const MemoryManager &mem, uint32_t team) : fMem(mem), fTeam(team) {}
	bool resolve(uint32_t handle, rg_bo_view *out) const override {
		const rg_bo *bo = fMem.lookup(fTeam, handle);
		if (bo == nullptr)
			return false;
		out->gpu_addr = bo->gpu_addr;
		out->size = bo->size;
		return true;
	}
private:
	const MemoryManager &fMem;
	uint32_t fTeam;
};

/* Policy registry for hot-swappable CS verification policies */
class PolicyRegistry {
public:
	PolicyRegistry() { register_defaults(); }
	
	void register_policy(const std::string &name, const rg_cs_policy &policy) {
		fPolicies[name] = policy;
	}
	
	const rg_cs_policy *get_policy(const std::string &name) const {
		auto it = fPolicies.find(name);
		return (it != fPolicies.end()) ? &it->second : nullptr;
	}
	
	void set_default(const std::string &name) {
		if (fPolicies.find(name) != fPolicies.end())
			fDefaultPolicy = name;
	}
	
	const rg_cs_policy &default_policy() const {
		auto it = fPolicies.find(fDefaultPolicy);
		return it != fPolicies.end() ? it->second : rg_policy_deny_all;
	}
	
private:
	void register_defaults() {
		fPolicies["deny-all"] = rg_policy_deny_all;
		fPolicies["test"] = rg_policy_test;
		fDefaultPolicy = "deny-all";
	}
	
	std::map<std::string, rg_cs_policy> fPolicies;
	std::string fDefaultPolicy;
};

/* Per-team state extended with policy selection */
struct rg_team_state {
	uint32_t vmid = 0;
	bool has_vmid = false;
	std::unique_ptr<GpuVm> vm;
	std::string policy_name;  // Policy name from registry
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
	ILogger &logger() { return *fLogger; }
	PolicyRegistry &policies() { return fPolicies; }

	/* Politica activa global: deny-all salvo que se instale la de test/produccion. */
	void set_policy(const rg_cs_policy &policy) { 
		fVerifier = CsVerifier(policy);
		fPolicies.register_policy("runtime", policy);
		fPolicies.set_default("runtime");
	}

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

	RingManager fRings;  // Must be declared before fMemory for initialization order
	MemoryManager fMemory;
	CsVerifier fVerifier;
	std::unique_ptr<ILogger> fLogger;
	PolicyRegistry fPolicies;
	VmidManager fVmids;
	std::map<uint32_t, rg_team_state> fTeams;
	std::mutex fTeamsLock;
	rg_asic_desc fAsic{};
};

#endif /* RG_IPC_SERVER_H */
