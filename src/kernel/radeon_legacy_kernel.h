/*
 * radeon_legacy_kernel.h - Haiku kernel module for legacy Radeon GPUs.
 *
 * This is a real legacy-style driver (init_hardware/publish_devices/
 * find_device/init_driver) that probes the PCI bus itself. It exposes the
 * RG_IOCTL_* contract from src/helper/helper_ioctls.h to userland.
 *
 * Display modesetting (AtomBIOS/DDC) is intentionally not implemented in the
 * standalone module; it is the integration point documented in
 * docs/HAIKU_INTEGRATION.md.
 */
#ifndef RADEON_LEGACY_KERNEL_H
#define RADEON_LEGACY_KERNEL_H

#include <Drivers.h>
#include <SupportDefs.h>

#define RG_LEGACY_DEVICE_PREFIX		"graphics/radeon_legacy_"
#define RG_LEGACY_ACCELERANT_NAME	"radeon_legacy.accelerant"
#define RG_LEGACY_MAX_DEVICES		4

/*
 * Legacy entry points. api_version must be B_CUR_DRIVER_API_VERSION (2).
 * These are the only symbols the kernel looks up in the add-on image.
 */
extern "C" {
status_t	init_hardware(void);
const char	**publish_devices(void);
device_hooks	*find_device(const char *name);
status_t	init_driver(void);
void		uninit_driver(void);
extern int32	api_version;
}

#endif	/* RADEON_LEGACY_KERNEL_H */
