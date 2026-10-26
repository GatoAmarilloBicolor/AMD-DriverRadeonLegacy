/*
 * helper_ioctls.h - Contrato de ioctls del helper de kernel (C1 / §5).
 * Compartido por radeon_hd (kernel) y el servidor (userland).
 */
#ifndef RG_HELPER_IOCTLS_H
#define RG_HELPER_IOCTLS_H

#include <stdint.h>
#include <stdbool.h>

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
	RG_IOCTL_IRQ_DETACH = 0x52470007,

	/* Enumeration / modesetting. The payloads are self-contained fixed-width
	 * structures so the kernel helper never has to include userland headers
	 * such as <Accelerant.h>. */
	RG_IOCTL_GET_CONNECTORS = 0x52470008,
	RG_IOCTL_GET_CRTCS = 0x52470009,
	RG_IOCTL_SET_CRTC = 0x5247000A,
	RG_IOCTL_PAGE_FLIP = 0x5247000B,
	RG_IOCTL_HOTPLUG_EVENT = 0x5247000C,
	RG_IOCTL_BPORT_TRANSPORT = 0x5247000D,};

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

/*
 * Display/modesetting contract.
 *
 * These structures are byte-for-byte ABI mirrors of the BeOS/Haiku
 * <Accelerant.h> types display_timing and display_mode. They are redeclared
 * here instead of including <Accelerant.h> so that this header stays usable
 * from the kernel helper, where userland OS headers must not be pulled in.
 * The userland accelerant converts between the two representations.
 * Verified against headers/os/add-ons/graphics/Accelerant.h (master):
 * display_timing is 24 bytes, display_mode is 40 bytes.
 * Sync polarity is encoded in the timing flags, never in extra fields.
 */
typedef struct rg_display_timing {
	uint32_t	pixel_clock;	/* kHz */
	uint16_t	h_display;		/* pixels */
	uint16_t	h_sync_start;
	uint16_t	h_sync_end;
	uint16_t	h_total;
	uint16_t	v_display;		/* lines */
	uint16_t	v_sync_start;
	uint16_t	v_sync_end;
	uint16_t	v_total;
	uint32_t	flags;			/* RG_TIMING_* */
} rg_display_timing;

typedef struct rg_display_mode {
	rg_display_timing timing;
	uint32_t	space;			/* pixel format (color_space) */
	uint16_t	virtual_width;	/* pixels */
	uint16_t	virtual_height;	/* lines */
	uint16_t	h_display_start;
	uint16_t	v_display_start;
	uint32_t	flags;			/* RG_MODE_* */
} rg_display_mode;

/*
 * Numeric mirrors of the Haiku <Accelerant.h> mode/timing flags. The values
 * are identical so a static_cast of the bitfields is safe in both directions.
 */
enum {
	RG_MODE_SCROLL			= 1u << 0,
	RG_MODE_8_BIT_DAC		= 1u << 1,
	RG_MODE_HARDWARE_CURSOR	= 1u << 2,
	RG_MODE_PARALLEL_ACCESS	= 1u << 3,
	RG_MODE_DPMS			= 1u << 4,
	RG_MODE_IO_FB_NA		= 1u << 5
};

enum {
	RG_DPMS_ON				= 1u << 0,
	RG_DPMS_STANDBY			= 1u << 1,
	RG_DPMS_SUSPEND			= 1u << 2,
	RG_DPMS_OFF				= 1u << 3
};

enum {
	RG_TIMING_BLANK_PEDESTAL	= 1u << 27,
	RG_TIMING_INTERLACED		= 1u << 28,
	RG_TIMING_POSITIVE_HSYNC	= 1u << 29,
	RG_TIMING_POSITIVE_VSYNC	= 1u << 30,
	RG_TIMING_SYNC_ON_GREEN		= 1u << 31
};

/* Display/Connector detection */
typedef struct rg_connector_info {
	uint32_t connector_id;
	uint32_t connector_type;  /* DRM_MODE_CONNECTOR_* */
	uint32_t crtc_id;         /* CRTC assigned, 0xFF if none */
	bool connected;
	bool active;
	uint32_t encoder_id;
	uint32_t dpms_mode;
	/* EDID data */
	uint8_t edid[256];
	uint32_t edid_size;
} rg_connector_info;

typedef struct rg_crtc_info {
	uint32_t crtc_id;
	bool active;
	uint32_t x, y;
	uint32_t width, height;
	uint32_t fb_id;           /* framebuffer ID */
	uint32_t gamma_size;
} rg_crtc_info;

typedef struct rg_encoder_info {
	uint32_t encoder_id;
	uint32_t encoder_type;
	uint32_t crtc_id;         /* CRTC bound to, 0xFF if none */
	uint32_t possible_crtcs;
	uint32_t possible_clones;
} rg_encoder_info;

/* GET_CONNECTORS */
typedef struct rg_get_connectors {
	uint32_t count;
	rg_connector_info connectors[16];
} rg_get_connectors;

/* GET_CRTCS */
typedef struct rg_get_crtcs {
	uint32_t count;
	rg_crtc_info crtcs[8];
} rg_get_crtcs;

/* SET_CRTC */
typedef struct rg_set_crtc {
	uint32_t crtc_id;
	uint32_t fb_id;
	uint32_t x, y;
	uint32_t width, height;
	uint32_t mode_valid;
	rg_display_mode mode;
} rg_set_crtc;

/* PAGE_FLIP */
typedef struct rg_page_flip {
	uint32_t crtc_id;
	uint32_t fb_id;
	uint32_t flags;
	uint32_t reserved;
} rg_page_flip;

/* HOTPLUG_EVENT */
typedef struct rg_hotplug_event {
	uint32_t connector_id;
	uint32_t connected;	/* 1 = connected, 0 = disconnected */
	uint32_t reserved[2];
} rg_hotplug_event;

/* Connector types (match DRM_MODE_CONNECTOR_*) */
#define DRM_MODE_CONNECTOR_Unknown     0
#define DRM_MODE_CONNECTOR_VGA         1
#define DRM_MODE_CONNECTOR_DVII        2
#define DRM_MODE_CONNECTOR_DVID        3
#define DRM_MODE_CONNECTOR_DVIA        4
#define DRM_MODE_CONNECTOR_Composite   5
#define DRM_MODE_CONNECTOR_SVIDEO      6
#define DRM_MODE_CONNECTOR_LVDS        7
#define DRM_MODE_CONNECTOR_Component   8
#define DRM_MODE_CONNECTOR_9PinDIN     9
#define DRM_MODE_CONNECTOR_DisplayPort 10
#define DRM_MODE_CONNECTOR_HDMIA       11
#define DRM_MODE_CONNECTOR_HDMIB       12
#define DRM_MODE_CONNECTOR_TV          13
#define DRM_MODE_CONNECTOR_eDP         14
#define DRM_MODE_CONNECTOR_VIRTUAL     15
#define DRM_MODE_CONNECTOR_DSI         16

/* DPMS modes */
#define DRM_MODE_DPMS_ON        0
#define DRM_MODE_DPMS_STANDBY   1
#define DRM_MODE_DPMS_SUSPEND   2
#define DRM_MODE_DPMS_OFF       3

/* Encoder types */
#define DRM_MODE_ENCODER_NONE     0
#define DRM_MODE_ENCODER_DAC      1
#define DRM_MODE_ENCODER_TMDS     2
#define DRM_MODE_ENCODER_LVDS     3
#define DRM_MODE_ENCODER_TVDAC    4
#define DRM_MODE_ENCODER_VIRTUAL  5
#define DRM_MODE_ENCODER_DSI      6
#define DRM_MODE_ENCODER_DPMST    7
#define DRM_MODE_ENCODER_DPI      8

#ifndef __HAIKU__
typedef int32_t status_t;
#endif

/* Clone kernel area into userland (server clones helper areas for clients) */
status_t rg_helper_clone_area(int32_t kernel_area, const char *name, void **user_addr);
status_t rg_ioctl_bport_transport(void *cookie, uint32_t code, void *buffer, size_t size);

/*
 * Payload handlers dispatched by the control hook. They are exported so the
 * accelerant and in-process contract tests can exercise them directly.
 */
status_t rg_ioctl_get_connectors(rg_get_connectors *connectors);
status_t rg_ioctl_get_crtcs(rg_get_crtcs *crtcs);
status_t rg_ioctl_set_crtc(rg_set_crtc *crtc);
status_t rg_ioctl_page_flip(rg_page_flip *flip);
status_t rg_ioctl_hotplug_event(rg_hotplug_event *event);

#ifdef __cplusplus
}
#endif

#endif /* RG_HELPER_IOCTLS_H */
