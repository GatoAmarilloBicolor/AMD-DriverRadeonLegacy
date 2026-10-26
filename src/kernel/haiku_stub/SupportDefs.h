/*
 * SupportDefs.h - Host-only stand-in for Haiku's <SupportDefs.h>.
 *
 * This header exists so the kernel module and the accelerant can be
 * syntax-checked on macOS (make haiku-check). It is NOT a Haiku ABI
 * replacement: the real build must use the Haiku SDK headers.
 */
#ifndef _SUPPORT_DEFS_H
#define _SUPPORT_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* Haiku integer aliases. */
typedef uint8_t		uint8;
typedef uint16_t	uint16;
typedef uint32_t	uint32;
typedef uint64_t	uint64;
typedef int8_t		int8;
typedef int16_t		int16;
typedef int32_t		int32;
typedef int64_t		int64;

typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned char		uchar_t;
typedef unsigned short		ushort_t;
typedef unsigned int		uint_t;
typedef unsigned long		ulong_t;

/* Haiku kernel object IDs are 32-bit. */
typedef int32	status_t;
typedef int32	area_id;
typedef int32	port_id;
typedef int32	sem_id;
typedef int32	team_id;
typedef int32	thread_id;
typedef int32	image_id;
typedef int32	mount_id;
typedef int32	disk_device_id;
typedef int32	partition_id;

typedef int64	bigtime_t;
typedef int64	nanotime_t;

typedef uint64	phys_addr_t;
typedef uint64	phys_size_t;
typedef uintptr_t	addr_t;

#define B_PAGE_SIZE			4096
#define B_PATH_NAME_LENGTH	1024
#define B_FILE_NAME_LENGTH	256
#define B_OS_NAME_LENGTH	32
#define B_ATTR_NAME_LENGTH	255
#define B_MIME_TYPE_LENGTH	255

/* printf format helpers (host uses the same widths as Haiku x86-64/arm64). */
#define B_PRId32	"d"
#define B_PRIi32	"i"
#define B_PRIu32	"u"
#define B_PRIo32	"o"
#define B_PRIx32	"x"
#define B_PRIX32	"X"
#define B_PRId64	"lld"
#define B_PRIi64	"lli"
#define B_PRIu64	"llu"
#define B_PRIo64	"llo"
#define B_PRIx64	"llx"
#define B_PRIX64	"llX"

/* Status codes (values are stub-local; only equality matters here). */
#define B_OK					((status_t)0)
#define B_ERROR					((status_t)-1)
#define B_BAD_VALUE				((status_t)-2)
#define B_BAD_TYPE				((status_t)-3)
#define B_BAD_INDEX				((status_t)-4)
#define B_BAD_HANDLE			((status_t)-5)
#define B_NO_MEMORY				((status_t)-6)
#define B_NO_INIT				((status_t)-7)
#define B_NOT_SUPPORTED			((status_t)-8)
#define B_NOT_ALLOWED			((status_t)-9)
#define B_PERMISSION_DENIED		((status_t)-10)
#define B_BUSY					((status_t)-11)
#define B_TIMED_OUT				((status_t)-12)
#define B_INTERRUPTED			((status_t)-13)
#define B_WOULD_BLOCK			((status_t)-14)
#define B_NAME_NOT_FOUND		((status_t)-15)
#define B_NAME_IN_USE			((status_t)-16)
#define B_NO_MORE_FDS			((status_t)-17)
#define B_BUFFER_OVERFLOW		((status_t)-18)
#define B_FILE_ERROR			((status_t)-19)
#define B_DEVICE_NOT_FOUND		((status_t)-20)
#define B_BAD_SEM_ID			((status_t)-21)
#define B_BAD_PORT_ID			((status_t)-22)

#endif	/* _SUPPORT_DEFS_H */
