/*
 * rg_legacy.h - Backend TeraScale / R300 (C2, ADR-009).
 *
 * Secuencia de arranque de §6.8 y clasificacion de compute de §8.14.3.
 * Las escrituras de registro concretas se delegan al ring/verificador; aqui
 * se modela el orden y se valida que host_coherent se respete (§6.10).
 */
#ifndef RG_LEGACY_H
#define RG_LEGACY_H

#include <stdint.h>
#include <vector>
#include <string>

#include "../../common/rg_types.h"
#include "../ring_manager.h"
#include "../firmware.h"

struct rg_startup_step {
	const char *name;
	int32_t status;
};

class RgLegacyDevice {
public:
	RgLegacyDevice(rg_family family, RingManager *ring, bool host_coherent);

	/* Ejecuta la secuencia completa; se detiene en el primer fallo. */
	int32_t startup();

	/* Prueba de anillo e IB: escribe/lee un scratch simulado. */
	int32_t ring_test(uint32_t pattern);
	int32_t ib_test();
	int32_t selftest_ring();

	const std::vector<rg_startup_step> &steps() const { return fSteps; }
	uint32_t scratch_value() const { return fScratch; }
	rg_family family() const { return fFamily; }

	/* Registros que este backend NO debe tocar (§6.7). */
	static bool owns_register(uint32_t reg);
	static const char *ownership_note();

private:
	int32_t step(const char *name, int32_t (RgLegacyDevice::*fn)());
	int32_t init_gart();
	int32_t init_mc();
	int32_t init_gpu_config();
	int32_t init_irq();
	int32_t init_firmware();
	int32_t init_cp();
	int32_t start_ring();

	rg_family fFamily;
	RingManager *fRing;
	bool fCoherent;
	uint32_t fScratch = 0;
	std::vector<rg_startup_step> fSteps;
};

/* §8.14.3: destino de un dispatch de compute. */
enum rg_dispatch_target {
	RG_DISPATCH_GPU_COMPUTE = 0,   /* compute nativo (Evergreen+) */
	RG_DISPATCH_GPU_GRAPHICS = 1,  /* emulacion por pipeline grafico (R600/R700) */
	RG_DISPATCH_CPU_LLVMPIPE = 2   /* fallback CPU */
};

/* shared_size: bytes de LDS/group memory usados por el kernel. */
rg_dispatch_target rg_classify_compute(rg_family family, uint32_t shared_size);

const char *rg_dispatch_target_name(rg_dispatch_target t);

#endif /* RG_LEGACY_H */
