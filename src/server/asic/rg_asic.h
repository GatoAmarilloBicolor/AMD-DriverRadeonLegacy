/*
 * rg_asic.h - Enumerador y base de datos de ASIC (C8 / §7A.3).
 *
 * La tabla se genera con scripts/gen_asic_db.py a partir de las fuentes de
 * Mesa/libdrm cuando estan disponibles; el archivo rg_asic_db.inc incluido es
 * una semilla verificable. Una GPU desconocida nunca recibe aceleracion.
 */
#ifndef RG_ASIC_H
#define RG_ASIC_H

#include <stdint.h>

#include "../../common/rg_types.h"

struct rg_ip_version {
	uint16_t type;
	uint8_t major;
	uint8_t minor;
	uint8_t rev;
};

enum rg_asic_feature {
	RG_ASIC_FEAT_VM   = 1u << 0,
	RG_ASIC_FEAT_SDMA = 1u << 1,
	RG_ASIC_FEAT_MEC  = 1u << 2,
	RG_ASIC_FEAT_PSP  = 1u << 3,
	RG_ASIC_FEAT_SMU  = 1u << 4
};

enum rg_vram_type {
	RG_VRAM_UNKNOWN = 0,
	RG_VRAM_DDR = 1,
	RG_VRAM_GDDR3 = 2,
	RG_VRAM_GDDR5 = 3,
	RG_VRAM_HBM = 4,
	RG_VRAM_HBM2 = 5,
	RG_VRAM_GDDR4 = 6,
	RG_VRAM_GDDR6 = 7,
	RG_VRAM_HBM3 = 8,
	RG_VRAM_DDR2 = 9,
	RG_VRAM_DDR3 = 10
};

typedef struct rg_asic_desc {
	const char *name;
	uint32_t device_id;
	uint32_t revision;
	uint32_t subsystem;
	rg_family family;
	uint32_t gfx_level;
	uint8_t  is_apu;
	uint8_t  vram_type;
	uint16_t pad;
	uint32_t features;
} rg_asic_desc;

#ifdef __cplusplus
extern "C" {
#endif

/* Busca por device id (revision/subsystem 0xFFFFFFFF actuan de comodin). */
const rg_asic_desc *rg_asic_lookup(uint32_t device_id, uint32_t revision,
	uint32_t subsystem);

/* Tabla completa y tamano (para tests de cobertura). */
const rg_asic_desc *rg_asic_table(uint32_t *count);

const char *rg_asic_family_name(rg_family family);

/* Descriptor minimo para una GPU no reconocida. */
extern const rg_asic_desc rg_asic_unknown;

#ifdef __cplusplus
}
#endif

#endif /* RG_ASIC_H */
