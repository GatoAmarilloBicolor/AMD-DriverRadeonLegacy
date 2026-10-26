/*
 * rg_ipc.h - Protocolo IPC shim <-> radeon_gpud (§6.1).
 *
 * Mensajes de tamano fijo por puerto. Los datos grandes (IBs, relocs) viajan
 * en areas compartidas y se referencian por area_id (int32 portable).
 */
#ifndef RG_IPC_H
#define RG_IPC_H

#include "rg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RG_IPC_MAGIC   0x52475044u  /* 'RGPD' */
#define RG_IPC_VERSION 1u

typedef enum rg_msg {
	RG_HELLO = 1,
	RG_INFO,
	RG_BO_CREATE,
	RG_BO_MMAP,
	RG_BO_WAIT_IDLE,
	RG_BO_SET_TILING,
	RG_BO_GET_TILING,
	RG_BO_BUSY,
	RG_BO_CLOSE,
	RG_CS,
	RG_FENCE_WAIT,
	RG_PING,
	RG_REPLY = 0x8000
} rg_msg;

/* area_id en Haiku es int32; en el host de desarrollo es un simple tag. */
#ifndef __HAIKU__
typedef int32_t area_id;
#endif

typedef struct rg_hdr {
	uint32_t magic;
	uint32_t version;
	uint32_t msg;
	uint32_t size;      /* bytes de payload */
	uint32_t team_id;   /* cliente, validado por el servidor */
	uint32_t seq;
} rg_hdr;

typedef struct rg_info_reply {
	uint32_t vendor_id;
	uint32_t device_id;
	uint32_t revision;
	uint32_t family;       /* rg_family */
	uint32_t host_coherent;/* §6.10 */
	uint32_t preferred;    /* 1 = preferred GPU, 0 = secondary */
	uint32_t screen_count; /* Number of screens */
	uint64_t mmio_phys;
	uint64_t mmio_size;
	uint64_t vram_phys;
	uint64_t vram_size;
	uint64_t gart_size;
	uint64_t tiling_capabilities; /* Bitfield of supported tiling formats */
} rg_info_reply;

typedef struct rg_bo_create {
	uint64_t size;
	uint32_t alignment;
	uint32_t domains;
	uint32_t flags;
	uint32_t tiling;
} rg_bo_create;

typedef struct rg_bo_reply {
	uint32_t status;   /* 0 o error negativo */
	uint32_t handle;
	area_id  area;
	uint64_t gpu_addr;
	uint64_t size;
} rg_bo_reply;

typedef struct rg_bo_tiling {
	uint32_t handle;
	uint32_t tiling;
} rg_bo_tiling;

typedef struct rg_bo_close {
	uint32_t handle;
	uint32_t pad;
} rg_bo_close;

typedef struct rg_reloc {
	uint32_t handle;
	uint32_t read_domain;
	uint32_t write_domain;
	uint32_t flags;
	uint64_t offset;       /* offset dentro del BO */
	uint64_t presumed_gpu_addr; /* direccion que el cliente asumio */
} rg_reloc;

typedef struct rg_cs {
	uint32_t ib_dwords;
	uint32_t n_relocs;
	area_id  ib_area;
	area_id  reloc_area;
	uint32_t ring;
	uint32_t flags;
	uint64_t fence_out;    /* direccion GTT donde la GPU escribe la fence */
} rg_cs;

typedef struct rg_cs_reply {
	int32_t  status;
	uint32_t pad;
	uint64_t fence;
} rg_cs_reply;

typedef struct rg_fence_wait {
	uint64_t fence;
	uint32_t timeout_ms;
	uint32_t pad;
} rg_fence_wait;

/* Tamano maximo de un mensaje fijo. */
#define RG_IPC_MAX_MSG 4096

#ifdef __cplusplus
}
#endif

#endif /* RG_IPC_H */
