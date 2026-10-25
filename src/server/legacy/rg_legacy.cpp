/*
 * rg_legacy.cpp - Backend legacy (R300..NI).
 */
#include "rg_legacy.h"

#include <cstring>

RgLegacyDevice::RgLegacyDevice(rg_family family, RingManager *ring, bool host_coherent)
	: fFamily(family), fRing(ring), fCoherent(host_coherent)
{
}

int32_t RgLegacyDevice::step(const char *name, int32_t (RgLegacyDevice::*fn)())
{
	int32_t st = (this->*fn)();
	fSteps.push_back({name, st});
	return st;
}

int32_t RgLegacyDevice::startup()
{
	fSteps.clear();
	int32_t st = step("gart", &RgLegacyDevice::init_gart);
	if (st != RG_OK) return st;
	st = step("memory-controller", &RgLegacyDevice::init_mc);
	if (st != RG_OK) return st;
	st = step("gpu-config", &RgLegacyDevice::init_gpu_config);
	if (st != RG_OK) return st;
	st = step("irq", &RgLegacyDevice::init_irq);
	if (st != RG_OK) return st;
	st = step("firmware", &RgLegacyDevice::init_firmware);
	if (st != RG_OK) return st;
	st = step("cp", &RgLegacyDevice::init_cp);
	if (st != RG_OK) return st;
	st = step("ring", &RgLegacyDevice::start_ring);
	if (st != RG_OK) return st;
	st = step("ring-test", &RgLegacyDevice::selftest_ring);
	if (st != RG_OK) return st;
	return step("ib-test", &RgLegacyDevice::ib_test);
}

/*
 * GART: en hardware se escribe la tabla de paginas con las direcciones que
 * devuelve TRANSLATE_PHYS. En el host el MemoryManager ya posee las paginas,
 * asi que aqui solo se valida la coherencia declarada por plataforma.
 */
int32_t RgLegacyDevice::selftest_ring() { return ring_test(0xA5A5A5A5u); }

int32_t RgLegacyDevice::init_gart()
{
	if (!fCoherent) {
		/* §6.10: en plataformas no coherentes, el ejecutor CPU trabaja sobre
		 * staging y el GTT se mapea sin cache. Debe estar configurado antes
		 * de cualquier DMA; si no, abortamos. */
		return RG_EOPNOTSUPP;
	}
	return RG_OK;
}

/* §6.7: solo lectura de MC_FB_LOCATION; no reconfigurar lo que dejo el BIOS. */
int32_t RgLegacyDevice::init_mc() { return RG_OK; }

int32_t RgLegacyDevice::init_gpu_config() { return RG_OK; }

int32_t RgLegacyDevice::init_irq() { return RG_OK; }

int32_t RgLegacyDevice::init_firmware()
{
	FirmwareLoader loader;
	std::vector<rg_fw_blob> blobs;
	std::vector<std::string> missing;
	int32_t n = loader.load_for(fFamily, &blobs, &missing);
	if (n <= 0)
		return RG_ENOENT;
	return RG_OK;
}

int32_t RgLegacyDevice::init_cp() { return RG_OK; }

int32_t RgLegacyDevice::start_ring()
{
	if (fRing == nullptr)
		return RG_EINVAL;
	uint32_t nop = 0xC0001000u; /* paquete NOP type-3 con opcode 0x10 */
	uint64_t fence = 0;
	return fRing->submit(RG_RING_GFX, &nop, 1, &fence);
}

int32_t RgLegacyDevice::ring_test(uint32_t pattern)
{
	if (fRing == nullptr)
		return RG_EINVAL;
	/* WRITE_DATA simulado: el valor aterriza en el scratch. */
	uint32_t ib[2] = { 0xC0010000u, pattern };
	uint64_t fence = 0;
	int32_t st = fRing->submit(RG_RING_GFX, ib, 2, &fence);
	if (st != RG_OK)
		return st;
	st = fRing->wait_fence(fence, 1000);
	if (st != RG_OK)
		return st;
	fScratch = pattern;
	return RG_OK;
}

int32_t RgLegacyDevice::ib_test()
{
	if (fRing == nullptr)
		return RG_EINVAL;
	uint32_t ib[4] = { 0xC0010000u, 0x12345678u, 0xC0010000u, 0x9ABCDEF0u };
	uint64_t fence = 0;
	int32_t st = fRing->submit(RG_RING_GFX, ib, 4, &fence);
	if (st != RG_OK)
		return st;
	return fRing->wait_fence(fence, 1000);
}

bool RgLegacyDevice::owns_register(uint32_t reg)
{
	/* Rangos reservados a radeon_hd/accelerant (CRTC, PLL, AtomBIOS, cursor).
	 * Cifras provisionales: ajustar con r600d.h antes de produccion. */
	if (reg >= 0x6000 && reg < 0x7000)
		return false; /* display */
	if (reg >= 0x0000 && reg < 0x0100)
		return false; /* VGA/CRTC legacy */
	return true;
}

const char *RgLegacyDevice::ownership_note()
{
	return "Ver docs/REGISTER_OWNERSHIP.md: display/PLL/AtomBIOS son de radeon_hd; "
	       "CP/GART/IH/3D son del servidor.";
}

rg_dispatch_target rg_classify_compute(rg_family family, uint32_t shared_size)
{
	if (family < RG_FAMILY_R600)
		return RG_DISPATCH_CPU_LLVMPIPE;
	if (family >= RG_FAMILY_EVERGREEN)
		return RG_DISPATCH_GPU_COMPUTE;
	/* R600/R700: sin LDS fiable para compute simple -> CPU. */
	if (shared_size > 0)
		return RG_DISPATCH_CPU_LLVMPIPE;
	return RG_DISPATCH_GPU_GRAPHICS;
}

const char *rg_dispatch_target_name(rg_dispatch_target t)
{
	switch (t) {
		case RG_DISPATCH_GPU_COMPUTE: return "gpu-compute";
		case RG_DISPATCH_GPU_GRAPHICS: return "gpu-graphics";
		case RG_DISPATCH_CPU_LLVMPIPE: return "cpu-llvmpipe";
	}
	return "?";
}
