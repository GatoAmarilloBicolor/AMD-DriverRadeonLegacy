/*
 * Drivers.h - Host-only stand-in for Haiku's <Drivers.h>.
 */
#ifndef _DRIVERS_H
#define _DRIVERS_H

#include <SupportDefs.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define B_CUR_DRIVER_API_VERSION	2

enum {
	B_GET_DEVICE_SIZE = 1,
	B_SET_DEVICE_SIZE,
	B_SET_NONBLOCKING_IO,
	B_SET_INTERRUPT_DRIVEN_IO,
	B_SET_HIGH_WATER_MARK,
	B_GET_DRIVER_FOR_DEVICE,
	B_SET_DEVICE_PARAMETERS,
	B_GET_DEVICE_PARAMETERS,
	B_GET_DEVICE_PARAMETERS_BY_NAME,
	B_SET_DMA_MODE,
	B_GET_DMA_MODE,
	B_GET_DMA_INFO,
	B_GET_DMA_STATUS,
	B_FIND_DEVICE,
	B_GET_DEVICE_NAME,
	B_GET_DRIVER_INFO,
	B_GET_MOUNT_POINT,
	B_GET_VOLUME_INFO,
	B_GET_MEDIA_STATUS,
	B_GET_PATH_FOR_DEVICE,
	B_GET_STAT,
	B_GET_PROTOCOL,
	B_GET_SHMEM_INFO,
	B_GET_SIGNATURE,
	B_GET_PARTITION_INFO,
	B_GET_ICON_NAME,
	B_READ_ATTRIBUTES,
	B_WRITE_ATTRIBUTES,
	B_GET_GEOMETRY,
	B_SET_GEOMETRY,
	B_GET_DRIVER_INFO_FOR_DEVICE,
	B_GET_DEVICE_INFO,
	B_GET_DEVICE_INFO_FOR_DEVICE,
	B_ACQUIRE_IO_RESERVATION,
	B_RELEASE_IO_RESERVATION,

	B_DEVICE_OP_CODES_END = 9999,
	B_GRAPHIC_DRIVER_BASE = 8300
};

typedef struct selectsync selectsync;
typedef struct iovec iovec;

typedef struct device_hooks {
	status_t	(*open)(const char *name, uint32 flags, void **cookie);
	status_t	(*close)(void *cookie);
	status_t	(*free)(void *cookie);
	status_t	(*control)(void *cookie, uint32 op, void *buffer, size_t length);
	status_t	(*read)(void *cookie, off_t pos, void *buffer, size_t *length);
	status_t	(*write)(void *cookie, off_t pos, const void *buffer, size_t *length);
	status_t	(*select)(void *cookie, uint8 event, uint32 ref, selectsync *sync);
	status_t	(*deselect)(void *cookie, uint8 event, selectsync *sync);
	status_t	(*readv)(void *cookie, off_t pos, const iovec *vec, size_t count,
					size_t *length);
	status_t	(*writev)(void *cookie, off_t pos, const iovec *vec, size_t count,
					size_t *length);
} device_hooks;

status_t	init_hardware(void);
const char	**publish_devices(void);
device_hooks	*find_device(const char *name);
status_t	init_driver(void);
void		uninit_driver(void);
extern int32	api_version;

#ifdef __cplusplus
}
#endif

#endif	/* _DRIVERS_H */
