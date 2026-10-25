/*
 * main.cpp - radeon_gpud (C2 / ADR-001).
 *
 * Modos:
 *   --enum       enumera la base de datos de ASIC y sale
 *   --selftest   arranca el servidor en memoria y valida subsistemas
 *   --serve      bucle de servicio (stdin se ignora; un BPort real lo
 *                sustituye en Haiku)
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ipc_server.h"
#include "asic/rg_asic.h"
#include "legacy/rg_legacy.h"

static int cmd_enum()
{
	uint32_t n = 0;
	const rg_asic_desc *table = rg_asic_table(&n);
	printf("ASIC database: %u entries\n", n);
	for (uint32_t i = 0; i < n; i++) {
		printf("  %08x rev %02x %-32s family=%-16s gfx=%u feat=0x%x\n",
			table[i].device_id, table[i].revision, table[i].name,
			rg_asic_family_name(table[i].family), table[i].gfx_level,
			table[i].features);
	}
	const rg_asic_desc *unknown = rg_asic_lookup(0xDEAD, 0, 0);
	printf("unknown probe -> %s (family=%s)\n", unknown->name,
		rg_asic_family_name(unknown->family));
	return 0;
}

static int cmd_selftest()
{
	int failures = 0;
	RgServer server(256ull << 20, 512ull << 20);

	/* BO + mapa */
	rg_bo_create req{};
	req.size = 8192;
	req.alignment = 4096;
	req.domains = RG_DOMAIN_GTT;
	req.flags = 0;
	rg_bo_reply rep{};
	int32_t st = server.handle(1, RG_BO_CREATE, &req, sizeof(req), &rep, sizeof(rep));
	printf("[%s] bo_create -> status=%d handle=%u gpu=0x%llx\n",
		st > 0 && rep.status == 0 ? "ok" : "FAIL", rep.status,
		rep.handle, (unsigned long long)rep.gpu_addr);
	failures += (st <= 0 || rep.status != 0);

	void *ptr = nullptr;
	st = server.handle(1, RG_BO_MMAP, &rep.handle, sizeof(rep.handle), &ptr, sizeof(ptr));
	printf("[%s] bo_mmap -> %p\n", st > 0 && ptr != nullptr ? "ok" : "FAIL", ptr);
	failures += (st <= 0 || ptr == nullptr);

	/* CS con politica deny-all: debe rechazar */
	uint32_t ib[2] = { 0xC0010000u, 0x1234u };
	uint8_t payload[sizeof(rg_cs) + sizeof(ib)] = {};
	rg_cs *cs = reinterpret_cast<rg_cs *>(payload);
	cs->ib_dwords = 2;
	cs->n_relocs = 0;
	cs->ring = RG_RING_GFX;
	memcpy(payload + sizeof(rg_cs), ib, sizeof(ib));
	rg_cs_reply csr{};
	st = server.handle(1, RG_CS, payload, sizeof(payload), &csr, sizeof(csr));
	printf("[%s] cs deny-all -> status=%d\n", csr.status != 0 ? "ok" : "FAIL", csr.status);
	failures += (csr.status == 0);

	/* Con politica de test: SET_CONFIG_REG con reloc a un BO propio */
	server.set_policy(rg_policy_test);
	rg_bo_create vreq{};
	vreq.size = 4096;
	vreq.domains = RG_DOMAIN_VRAM;
	rg_bo_reply vrep{};
	server.handle(1, RG_BO_CREATE, &vreq, sizeof(vreq), &vrep, sizeof(vrep));

	uint32_t ib2[3] = { (3u << 30) | (0x14u << 8) | 2u, 0x2000u, 0u };
	rg_reloc rel{};
	rel.handle = vrep.handle;
	rel.offset = 0;
	uint8_t payload2[sizeof(rg_cs) + sizeof(ib2) + sizeof(rel)] = {};
	rg_cs *cs2 = reinterpret_cast<rg_cs *>(payload2);
	cs2->ib_dwords = 3;
	cs2->n_relocs = 1;
	memcpy(payload2 + sizeof(rg_cs), ib2, sizeof(ib2));
	memcpy(payload2 + sizeof(rg_cs) + sizeof(ib2), &rel, sizeof(rel));
	st = server.handle(1, RG_CS, payload2, sizeof(payload2), &csr, sizeof(csr));
	printf("[%s] cs patched -> status=%d fence=%llu\n",
		csr.status == 0 ? "ok" : "FAIL", csr.status, (unsigned long long)csr.fence);
	failures += (csr.status != 0);

	/* Aislamiento de handles entre equipos */
	uint32_t busy = 0;
	st = server.handle(2, RG_BO_BUSY, &vrep.handle, sizeof(vrep.handle), &busy, sizeof(busy));
	int32_t code = 0;
	memcpy(&code, &busy, sizeof(code));
	printf("[%s] cross-team denied -> %d\n", code != 0 ? "ok" : "FAIL", code);
	failures += (code == 0);

	server.drop_team(1);
	server.drop_team(2);

	/* Backend legacy: arranque y compute */
	RgLegacyDevice dev(RG_FAMILY_R600, &server.rings(), true);
	st = dev.startup();
	printf("[%s] legacy startup -> status=%d steps=%zu\n", st == RG_OK ? "ok" : "warn",
		st, dev.steps().size());
	rg_dispatch_target t = rg_classify_compute(RG_FAMILY_R600, 0);
	printf("[%s] classify R600 map-compute -> %s\n",
		t == RG_DISPATCH_GPU_GRAPHICS ? "ok" : "FAIL", rg_dispatch_target_name(t));
	failures += (t != RG_DISPATCH_GPU_GRAPHICS);

	printf("selftest: %d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}

static int cmd_serve()
{
	RgServer server(256ull << 20, 512ull << 20);
	printf("radeon_gpud: listo (vram=256MiB gart=512MiB)\n");
	printf("radeon_gpud: en Haiku se sustituye este bucle por un BPort loop\n");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--enum") == 0)
		return cmd_enum();
	if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
		return cmd_selftest();
	return cmd_serve();
}
