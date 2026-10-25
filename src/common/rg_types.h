/*
 * rg_types.h - Tipos base compartidos (C y C++).
 * Sin dependencias de Haiku: este header se usa en servidor, shim y tests.
 */
#ifndef RG_TYPES_H
#define RG_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dominios de memoria (equivalen a RADEON_GEM_DOMAIN_*) */
typedef enum rg_domain {
	RG_DOMAIN_CPU  = 0,
	RG_DOMAIN_GTT  = 1,
	RG_DOMAIN_VRAM = 2
} rg_domain;

#define RG_DOMAIN_MASK 0x3u

/* Familias soportadas. El orden importa: el clasificador compara rangos. */
typedef enum rg_family {
	RG_FAMILY_UNKNOWN = 0,
	RG_FAMILY_R300 = 1,        /* R300-R500 (pre-TeraScale) */
	RG_FAMILY_R600 = 2,        /* R600/R700 (TeraScale 1) */
	RG_FAMILY_EVERGREEN = 3,   /* Evergreen (TeraScale 2) */
	RG_FAMILY_NI = 4,          /* Northern Islands (TeraScale 3) */
	RG_FAMILY_GCN1 = 5,        /* GFX6 */
	RG_FAMILY_GCN2 = 6,        /* GFX7 */
	RG_FAMILY_GCN3 = 7,        /* GFX8 */
	RG_FAMILY_GCN5 = 8,        /* GFX9 */
	RG_FAMILY_RDNA1 = 9,       /* GFX10.1 */
	RG_FAMILY_RDNA2 = 10,      /* GFX10.3 */
	RG_FAMILY_RDNA3 = 11,      /* GFX11 */
	RG_FAMILY_RDNA4 = 12       /* GFX12 */
} rg_family;

/* Nivel GFX mayor para decisiones de backend. */
static inline uint32_t rg_family_gfx_level(rg_family f)
{
	switch (f) {
		case RG_FAMILY_GCN1: return 6;
		case RG_FAMILY_GCN2: return 7;
		case RG_FAMILY_GCN3: return 8;
		case RG_FAMILY_GCN5: return 9;
		case RG_FAMILY_RDNA1: return 10;
		case RG_FAMILY_RDNA2: return 10;
		case RG_FAMILY_RDNA3: return 11;
		case RG_FAMILY_RDNA4: return 12;
		default: return 0;
	}
}

static inline int rg_family_is_legacy(rg_family f)
{
	return f >= RG_FAMILY_R300 && f <= RG_FAMILY_NI;
}

static inline int rg_family_is_amdgpu(rg_family f)
{
	return f >= RG_FAMILY_GCN1;
}

/* Errores estilo Linux (negativos), independientes de la plataforma. */
enum {
	RG_OK = 0,
	RG_EINVAL = -22,
	RG_EACCES = -13,
	RG_EPERM = -1,
	RG_ENOMEM = -12,
	RG_ENODEV = -19,
	RG_ENOENT = -2,
	RG_EBUSY = -16,
	RG_ECANCELED = -125,
	RG_EOPNOTSUPP = -95
};

/* utilidades */
static inline uint64_t rg_align_up(uint64_t v, uint64_t a)
{
	return a ? (v + a - 1) & ~(a - 1) : v;
}

static inline uint64_t rg_align_down(uint64_t v, uint64_t a)
{
	return a ? v & ~(a - 1) : v;
}

static inline int rg_is_pow2(uint64_t v)
{
	return v && (v & (v - 1)) == 0;
}

/* Flags de IOCTL compartidos kernel<->userland (no dependen de Haiku). */
enum {
	RG_MAP_READ  = 1u << 0,
	RG_MAP_WRITE = 1u << 1,
	RG_MAP_WC    = 1u << 2
};

/* Firmware: maxima cantidad de blobs por ASIC */
#define RG_FW_MAX_BLOBS 8
#define RG_FW_NAME_MAX  64

#ifdef __cplusplus
}
#endif

#endif /* RG_TYPES_H */
