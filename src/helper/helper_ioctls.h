/*
 * helper_ioctls.h - Contrato de ioctls del helper de kernel (C1 / §5).
 * Compartido por radeon_hd (kernel) y el servidor (userland).
 */
#ifndef RG_HELPER_IOCTLS_H
#define RG_HELPER_IOCTLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Produccion: el helper forma parte del radeon_hd nativo de Haiku, que ya
 * posee modesetting/AtomBIOS (ADR-004). Para un modulo de desarrollo aislado
 * se compila con -DRG_HELPER_DEVICE_NAME=\"graphics/radeon_legacy\".
 */
#ifndef RG_HELPER_DEVICE_NAME
#  ifdef RG_STANDALONE_HELPER
#    define RG_HELPER_DEVICE_NAME "graphics/radeon_legacy"
#  else
#    define RG_HELPER_DEVICE_NAME "graphics/radeon_hd"
#  endif
#endif

/*
 * Codigos de ioctl. Son valores altos para no colisionar con ioctls
 * genericos; Haiku los entrega tal cual al control_hook del driver.
 */
enum rg_helper_ioctl {
	RG_IOCTL_GET_INFO = 0x52470001,
	RG_IOCTL_MAP_MMIO = 0x52470002,
	RG_IOCTL_MAP_VRAM = 0x52470003,
	RG_IOCTL_TRANSLATE_PHYS = 0x52470004,
	RG_IOCTL_IRQ_ATTACH = 0x52470005,
	RG_IOCTL_PCI_CFG = 0x52470006,
	RG_IOCTL_IRQ_DETACH = 0x52470007
};

/* GET_INFO */
typedef struct rg_kernel_info {
	uint32_t vendor_id;
	uint32_t device_id;
	uint32_t revision;
	uint32_t host_coherent;
	uint64_t mmio_phys;
	uint64_t mmio_size;
	uint64_t vram_phys;
	uint64_t vram_size;
	uint64_t gart_base;   /* ventana GTT visible por la GPU */
	uint64_t gart_size;
	uint32_t pci_bus;
	uint32_t pci_device;
	uint32_t pci_function;
	uint32_t irq;
} rg_kernel_info;

/* MAP_MMIO / MAP_VRAM: entrada vacia, salida area_id */
typedef struct rg_map_request {
	uint32_t flags;   /* RG_MAP_* */
	uint32_t pad;
	int32_t  area;    /* out */
} rg_map_request;

/*
 * TRANSLATE_PHYS: area_id + offset + len -> lista de (phys,len).
 *
 * `area` sirve para las areas que crea el propio helper (MMIO/VRAM). Para
 * areas del servidor (BOs en GTT) se envia `address`, el puntero virtual que
 * el servidor obtuvo al clonar el area; el helper usa get_memory_map().
 */
#define RG_TRANSLATE_MAX_RANGES 64
typedef struct rg_translate_request {
	int32_t  area;
	uint32_t pad;
	uint64_t offset;
	uint64_t length;
	uint64_t address;   /* in: puntero virtual (si area == 0/-1) */
	uint32_t n_ranges;  /* out */
	uint32_t pad2;
	struct {
		uint64_t phys;
		uint64_t length;
	} ranges[RG_TRANSLATE_MAX_RANGES];
} rg_translate_request;

/* IRQ_ATTACH / IRQ_DETACH */
typedef struct rg_irq_request {
	int32_t  sem;         /* sem_id que el kernel libera */
	uint32_t status_reg;  /* offset MMIO de estado */
	uint32_t mask;        /* bits que nos pertenecen */
	uint32_t flags;       /* RG_IRQ_* */
} rg_irq_request;

enum {
	RG_IRQ_LEVEL = 0,
	RG_IRQ_EDGE  = 1
};

/* PCI_CFG */
typedef struct rg_pci_cfg_request {
	uint32_t offset;
	uint32_t length;
	uint32_t write;   /* 0 = lectura, 1 = escritura */
	uint32_t pad;
	uint8_t  data[256];
} rg_pci_cfg_request;

#ifndef __HAIKU__
typedef int32_t status_t;
#endif

/* Clone kernel area into userland (server clones helper areas for clients) */
status_t rg_helper_clone_area(int32_t kernel_area, const char *name, void **user_addr);

#ifdef __cplusplus
}
#endif

#endif /* RG_HELPER_IOCTLS_H */
