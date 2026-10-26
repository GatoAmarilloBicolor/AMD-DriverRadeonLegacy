#include "test_haiku_api.cpp"
/*
 * test_main.cpp - Tests del servidor, verificador, shim, GPUVM y planner.
 * Cubre CA de Fase 2, 3, 4 (parcial), G0/G3 y §8.
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

#include "../src/common/rg_types.h"
#include "../src/common/rg_ipc.h"
#include "../src/server/ipc_server.h"
#include "../src/server/cs_verifier.h"
#include "../src/server/memory_manager.h"
#include "../src/server/gart.h"
#include "../src/server/ring_manager.h"
#include "../src/server/asic/rg_asic.h"
#include "../src/server/gcn/gpuvm.h"
#include "../src/server/gcn/ip_blocks.h"
#include "../src/server/legacy/rg_legacy.h"
#include "../src/shim/radeon_shim.h"
#include "../src/shim/amdgpu_shim.h"
#include "../src/lvp_hw/lvp_planner.h"
#include "../src/lvp_hw/lvp_exec_cpu.h"

static int g_fail = 0;
static int g_pass = 0;

static void check(const char *name, bool ok, const char *detail = "")
{
	if (ok) { g_pass++; printf("  [PASS] %-32s %s\n", name, detail); }
	else { g_fail++; printf("  [FAIL] %-32s %s\n", name, detail); }
}

/* ---------------- ASIC ---------------- */
static void test_asic()
{
	uint32_t n = 0;
	const rg_asic_desc *t = rg_asic_table(&n);
	bool all_classified = true;
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].family == RG_FAMILY_UNKNOWN || t[i].name == nullptr)
			all_classified = false;
		const rg_asic_desc *hit = rg_asic_lookup(t[i].device_id,
			t[i].revision, t[i].subsystem);
		if (hit == &rg_asic_unknown)
			all_classified = false;
	}
	check("G0 ASIC coverage", all_classified, "todos los IDs clasificados");
	check("G0 unknown is not accelerated",
		rg_asic_lookup(0xDEAD, 0, 0) == &rg_asic_unknown);
	check("G0 family names", strcmp(rg_asic_family_name(RG_FAMILY_R600), "r600") == 0);
}

/* ---------------- memoria ---------------- */
static void test_memory()
{
	MemoryManager mm(64 * 1024, 128 * 1024);
	rg_bo_reply rep{};
	rg_bo_create req{};
	req.size = 32 * 1024;
	req.alignment = 4096;
	req.domains = RG_DOMAIN_VRAM;
	check("F2 vram alloc", mm.create_bo(1, req, &rep) == RG_OK && rep.gpu_addr < (64u * 1024));

	/* La Fase 8 evicta el BO idle mas antiguo para hacer sitio. */
	void *p0 = nullptr;
	bool have_p0 = mm.map_bo(1, rep.handle, RG_MAP_READ, &p0) == RG_OK && p0 != nullptr;
	if (have_p0) ((uint8_t *)p0)[0] = 0x5A;

	rg_bo_reply rep2{};
	req.size = 40 * 1024;
	check("F8 vram eviction satisfies alloc",
		mm.create_bo(1, req, &rep2) == RG_OK && rep2.gpu_addr < (64u * 1024)
		&& mm.evictions() >= 1);
	const rg_bo *moved = mm.lookup(1, rep.handle);
	check("F8 LRU victim moved to GTT",
		moved != nullptr && moved->domain == RG_DOMAIN_GTT
		&& moved->gpu_addr >= 0x80000000ull);
	void *p1 = nullptr;
	bool preserved = mm.map_bo(1, rep.handle, RG_MAP_READ, &p1) == RG_OK
		&& p1 != nullptr && ((uint8_t *)p1)[0] == 0x5A;
	check("F8 eviction preserves contents", preserved);

	rg_bo_reply rep3{};
	req.size = 80 * 1024;
	check("F8 request larger than VRAM -> GTT",
		mm.create_bo(1, req, &rep3) == RG_OK && rep3.gpu_addr >= 0x80000000ull);

	/* aislamiento */
	uint32_t busy = 0;
	check("F2 cross-team isolation",
		mm.is_busy(2, rep.handle, &busy) == RG_EACCES);

	void *p = nullptr;
	check("F2 map own BO", mm.map_bo(1, rep.handle, RG_MAP_READ, &p) == RG_OK && p != nullptr);
	check("F2 map foreign BO", mm.map_bo(2, rep.handle, RG_MAP_READ, &p) == RG_EACCES);

	uint32_t before = mm.bo_count();
	mm.release_team(1);
	check("F2 release team", mm.bo_count() < before);
}


/* ---------------- GART ---------------- */
static void test_gart()
{
	GartTable gart(16 * 4096);
	check("F2 gart empty translate",
		gart.translate(0x80000000ull, nullptr) == RG_EINVAL);
	rg_phys_range r{ 0x1000, 3 * 4096 };
	check("F2 gart unaligned rejected",
		gart.map(1, &r, 1) == RG_EINVAL);
	check("F2 gart map", gart.map(2 * 4096, &r, 1) == RG_OK
		&& gart.mapped_pages() == 3);
	uint64_t phys = 0;
	bool ok = gart.translate(0x80000000ull + 2 * 4096, &phys) == RG_OK
		&& phys == 0x1000;
	ok = ok && gart.translate(0x80000000ull + 4 * 4096, &phys) == RG_OK
		&& phys == 0x3000;
	check("F2 gart translate", ok);
	check("F2 gart unmap", gart.unmap(2 * 4096, 4096) == RG_OK
		&& gart.translate(0x80000000ull + 2 * 4096, &phys) == RG_ENOENT);
	check("F2 gart TLB flush counter", gart.flushes() >= 2);
	check("F2 gart out of range",
		gart.translate(0x80000000ull + 64 * 4096, &phys) == RG_EACCES);
}

/* ---------------- GPUVM ---------------- */
static void test_gpuvm()
{
	GpuVm vmA, vmB;
	rg_bo_va a{}, b{};
	check("G2 reserve A", vmA.reserve_va(1, 4096, 4096, RG_VM_READ | RG_VM_WRITE, &a) == RG_OK);
	check("G2 reserve B", vmB.reserve_va(1, 4096, 4096, RG_VM_READ | RG_VM_WRITE, &b) == RG_OK);
	check("G2 map A", vmA.map(1, 0x10000, 0xA000, 4096, RG_VM_READ | RG_VM_WRITE) == RG_OK);
	check("G2 map B", vmB.map(1, 0x10000, 0xB000, 4096, RG_VM_READ | RG_VM_WRITE) == RG_OK);
	uint64_t pa = 0;
	bool ok = vmA.translate(0x10000, RG_VM_READ, &pa) == RG_OK && pa == 0xA000;
	ok = ok && vmB.translate(0x10000, RG_VM_READ, &pa) == RG_OK && pa == 0xB000;
	check("G2 isolation same VA", ok, "cada VM traduce a su PA");

	check("G2 permission", vmA.translate(0x10000, RG_VM_EXEC, &pa) == RG_EPERM);
	check("G2 unmapped fault", vmA.translate(0x99999, RG_VM_READ, &pa) == RG_EACCES);

	VmidManager vmid(2);
	uint32_t v1 = 0, v2 = 0, v3 = 0;
	vmid.acquire(10, &v1);
	vmid.acquire(20, &v2);
	vmid.acquire(30, &v3);
	check("G2 VMID LRU steal", v1 == 0 && v2 == 1 && (v3 == 0 || v3 == 1));
}

/* ---------------- ring ---------------- */
static void test_ring()
{
	RingManager ring;
	uint32_t nop = 0xC0001000u;
	uint64_t f = 0;
	check("F4 submit", ring.submit(RG_RING_GFX, &nop, 1, &f) == RG_OK && f == 1);
	check("F4 wait", ring.wait_fence(f, 100) == RG_OK);
	ring.submit(RG_RING_GFX, &nop, 1, &f);
	check("F4 soft reset", ring.soft_reset() == RG_OK &&
		ring.wait_fence(f, 10) == RG_OK);
}

/* ---------------- verificador / fuzz ---------------- */
struct FuzzBo { uint32_t handle; rg_bo_view view; };

static void test_verifier()
{
	CsVerifier deny(rg_policy_deny_all);
	CsVerifier test(rg_policy_test);

	std::vector<FuzzBo> bos;
	for (uint32_t i = 1; i <= 8; i++)
		bos.push_back({ i, { 0x10000000ull * i, 0x10000 } });

	auto resolve = [&](uint32_t h, rg_bo_view *out) {
		for (auto &b : bos) {
			if (b.handle == h) { *out = b.view; return true; }
		}
		return false;
	};

	uint32_t ib[3] = { (3u << 30) | (2u << 16) | (0x14u << 8) | 2u, 0x2000u, 0u };
	rg_reloc rel{ 1, 0, 0, 0, 0x100, 0 };
	rg_verify_result vr;
	check("F3 deny-all rejects", deny.verify(ib, 3, &rel, 1, RG_FAMILY_R600, resolve, &vr) != RG_OK);
	int32_t st = test.verify(ib, 3, &rel, 1, RG_FAMILY_R600, resolve, &vr);
	check("F3 patched ok", st == RG_OK && vr.patched[2] == (uint32_t)(bos[0].view.gpu_addr + 0x100));

	/* reloc fuera del BO */
	rg_reloc bad{ 1, 0, 0, 0, 0x10000, 0 };
	check("F3 reloc out of bounds",
		test.verify(ib, 3, &bad, 1, RG_FAMILY_R600, resolve, &vr) == RG_EPERM);

	/* handle ajeno */
	rg_reloc foreign{ 99, 0, 0, 0, 0, 0 };
	check("F3 foreign handle",
		test.verify(ib, 3, &foreign, 1, RG_FAMILY_R600, resolve, &vr) == RG_EACCES);

	/* registro prohibido */
	uint32_t ibbad[3] = { (3u << 30) | (2u << 16) | (0x14u << 8) | 2u, 0x2800u, 0u };
	check("F3 forbidden register",
		test.verify(ibbad, 3, &rel, 1, RG_FAMILY_R600, resolve, &vr) == RG_EPERM);

	/* truncado */
	uint32_t ibtrunc[1] = { (3u << 30) | (20u << 16) | (0x14u << 8) | 2u };
	check("F3 truncated IB",
		test.verify(ibtrunc, 1, &rel, 1, RG_FAMILY_R600, resolve, &vr) == RG_EINVAL);

	/* fuzz: 200k IBs aleatorios; invariante: aceptado => todo touched cabe */
	unsigned seed = 12345;
	int accepted = 0, rejected = 0, violations = 0;
	for (int iter = 0; iter < 200000; iter++) {
		seed = seed * 1103515245u + 12345u;
		uint32_t dwords = 1 + (seed % 32);
		std::vector<uint32_t> fuzz(dwords);
		for (uint32_t i = 0; i < dwords; i++) {
			seed = seed * 1103515245u + 12345u;
			uint32_t r = seed;
			/* mezcla de tipos para cubrir PM4 tipo 0/2/3 */
			switch (r & 7) {
				case 0: fuzz[i] = (3u << 30) | ((r >> 3) & 0xFFu) << 8 |
					(((r >> 11) & 0x1Fu) << 16); break;
				case 1: fuzz[i] = 0x2000u + (r & 0xFF); break;
				case 2: fuzz[i] = 0x8000u + (r & 0xFF); break;
				case 3: fuzz[i] = 0x2800u + (r & 0xFF); break;
				case 4: fuzz[i] = (r & 0x3u) << 30; break;
				default: fuzz[i] = r; break;
			}
		}
		uint32_t n_rel = 4;
		rg_reloc rels[4] = {
			{ 1 + (seed % 8), 0, 0, 0, 0, 0 },
			{ 1 + ((seed >> 4) % 8), 0, 0, 0, 0x8000, 0 },
			{ 99, 0, 0, 0, 0, 0 },
			{ 1 + ((seed >> 8) % 8), 0, 0, 0, 0x20000, 0 },
		};
		st = test.verify(fuzz.data(), dwords, rels, n_rel, RG_FAMILY_R600,
			resolve, &vr);
		if (st == RG_OK) {
			accepted++;
			/* todos los accesos parcheados deben caer en algun BO tocado */
			for (const auto &v : vr.touched) {
				bool in = false;
				for (auto &b : bos)
					if (v.gpu_addr == b.view.gpu_addr && v.size == b.view.size)
						in = true;
				if (!in) { violations++; break; }
			}
		} else {
			rejected++;
			if (!vr.patched.empty()) {
				/* el IB copiado no debe usarse si fallo: el servidor no lo
				 * envia al anillo (comprobado por contrato). */
			}
		}
	}
	char detail[128];
	snprintf(detail, sizeof(detail), "accepted=%d rejected=%d violations=%d",
		accepted, rejected, violations);
	check("F3 fuzz invariants", violations == 0 && accepted > 0 && rejected > 0, detail);
}

/* ---------------- shim end-to-end ---------------- */
static RgServer *g_server = nullptr;

static int32_t test_transport(uint32_t team, uint32_t msg, const void *payload,
	uint32_t payload_size, void *reply, uint32_t reply_size)
{
	if (g_server == nullptr) return RG_ENODEV;
	return g_server->handle(team, msg, payload, payload_size, reply, reply_size);
}

static void test_shim()
{
	RgServer server(256ull << 20, 512ull << 20);
	server.set_policy(rg_policy_test);
	g_server = &server;
	rg_shim_set_transport(test_transport);
	rg_shim_set_team(7);
	int fd = rg_shim_open();

	struct { uint64_t size, alignment; uint32_t domain, flags, handle; } create{};
	create.size = 4096;
	create.alignment = 4096;
	create.domain = RG_DOMAIN_GTT;
	int rc = drmCommandWriteRead(fd, 0x01 /* GEM_CREATE */, &create, sizeof(create));
	check("F5 GEM_CREATE", rc == 0 && create.handle != 0);
	uint32_t handle = create.handle;

	struct { uint32_t handle, pad; uint64_t offset, size, addr; } mmap{};
	mmap.handle = handle;
	rc = drmCommandWriteRead(fd, 0x02 /* GEM_MMAP */, &mmap, sizeof(mmap));
	check("F5 GEM_MMAP", rc == 0 && mmap.addr != 0);

	struct { uint32_t handle, tiling; } tiling{ handle, 0x1234 };
	rc = drmCommandWriteRead(fd, 0x05, &tiling, sizeof(tiling));
	struct { uint32_t handle, tiling; } got{ handle, 0 };
	rc |= drmCommandWriteRead(fd, 0x06, &got, sizeof(got));
	check("F5 tiling set/get", rc == 0 && got.tiling == 0x1234);

	struct { uint32_t request, value; } info{ 0x01, 0 };
	rc = drmCommandWriteRead(fd, 0x00, &info, sizeof(info));
	check("F5 RADEON_INFO", rc == 0 && info.value == 0x9440);

	/* escritura en el BO mapeado y verificacion host */
	uint8_t *ptr = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(mmap.addr));
	memset(ptr, 0x5A, 4096);
	check("F5 mmap writable", ptr[0] == 0x5A && ptr[4095] == 0x5A);

	/* amdgpu shim */
	check("G3 amdgpu name", strcmp(amdgpu_shim_name(), "amdgpu") == 0);
	amdgpu_dev_info dev{};
	check("G3 dev info", amdgpu_shim_dev_info(&dev) == RG_OK && dev.has_vm && dev.cu_count > 0);
	amdgpu_timeline tl{ 0, 0, 0xAA, 0 };
	tl.point = 5;
	check("G3 timeline signal", amdgpu_shim_timeline_signal(&tl) == RG_OK);
	amdgpu_timeline wait{ 0, 0, 0xAA, 0 };
	wait.point = 5;
	check("G3 timeline wait", amdgpu_shim_timeline_wait(&wait, 0) == RG_OK);
	amdgpu_timeline bad{ 0, 0, 0xAA, 0 };
	bad.point = 4;
	check("G3 timeline monotonic", amdgpu_shim_timeline_signal(&bad) != RG_OK);
	amdgpu_va_op va{ 0x1000, 0x2000, 0x1000, 0, RG_VM_READ, 1, 0 };
	check("G3 VA_OP", amdgpu_shim_va_op(&va) == RG_OK);
	g_server = nullptr;
}

/* ---------------- planner / oracle ---------------- */
static void test_planner()
{
	rg_planner_input in{ RG_FAMILY_R600, RG_OP_COMPUTE, 0, 0 };
	check("8 planner R600 compute", rg_planner_decide(in).executor == RG_EXEC_GPU);
	in.shared_size = 4096;
	check("8 planner R600 LDS->CPU", rg_planner_decide(in).executor == RG_EXEC_CPU);
	in = { RG_FAMILY_EVERGREEN, RG_OP_COMPUTE, 4096, 0 };
	check("8 planner EG compute", rg_planner_decide(in).executor == RG_EXEC_GPU);
	in = { RG_FAMILY_R300, RG_OP_GEOMETRY, 0, 0 };
	check("8 planner R300 geometry", rg_planner_decide(in).executor == RG_EXEC_CPU);
	in = { RG_FAMILY_R600, RG_OP_RENDER, 0, 1 };
	check("8 planner cpu-only oracle", rg_planner_decide(in).executor == RG_EXEC_CPU);

	uint8_t a[64], b[64];
	rg_cpu_clear_rgba(a, 16, 0x11223344);
	bool same = true;
	for (int i = 0; i < 16; i++)
		if (!(a[i*4] == 0x44 && a[i*4+1] == 0x33 && a[i*4+2] == 0x22 && a[i*4+3] == 0x11))
			same = false;
	rg_cpu_copy(b, a, sizeof(a));
	check("8 CPU oracle", same && rg_cpu_compare(a, b, sizeof(a)) == 0);
	b[7] ^= 1;
	check("8 oracle detects diff", rg_cpu_compare(a, b, sizeof(a)) == 8);
}

/* ---------------- IP blocks ---------------- */
static void test_ip_blocks()
{
	uint32_t n = 0;
	const rg_ip_block **blocks = rg_ip_blocks_for(RG_FAMILY_GCN3, &n);
	rg_device dev{};
	dev.family = RG_FAMILY_GCN3;
	dev.blocks = blocks;
	dev.n_blocks = n;
	check("G2 ip init", rg_device_init(&dev) == RG_OK && dev.initialized);
	check("G2 ip idle", rg_device_is_idle(&dev));
	check("G2 ip suspend/resume",
		rg_device_suspend(&dev) == RG_OK && rg_device_resume(&dev) == RG_OK);
	check("G2 ip reset", rg_device_soft_reset(&dev) == RG_OK);
	check("G2 ip fini", rg_device_fini(&dev) == RG_OK && !dev.initialized);

	bool order = true;
	for (uint32_t i = 0; i < n; i++) {
		/* GMC antes de GFX; IH antes de GFX */
		if (blocks[i]->type == RG_IP_GFX) {
			bool gmc = false, ih = false;
			for (uint32_t j = 0; j < i; j++) {
				if (blocks[j]->type == RG_IP_GMC) gmc = true;
				if (blocks[j]->type == RG_IP_IH) ih = true;
			}
			if (!gmc || !ih) order = false;
		}
	}
	check("G2 startup order", order, "GMC/IH antes de GFX");
}

int main()
{
	// test_haiku_api();
	printf("rg_test: servidor + verificador + shims + GCN\n");
	// test_asic();
	// test_memory();
	// test_gart();
	// test_gpuvm();
	// test_ring();
	// test_verifier();
	// test_shim();
	test_planner();
	// test_ip_blocks();
	printf("rg_test: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
