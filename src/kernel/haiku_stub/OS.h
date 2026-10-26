/*
 * OS.h - Host-only stand-in for Haiku's <OS.h> (kernel subset).
 */
#ifndef _OS_H
#define _OS_H

#include <SupportDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define B_NO_LOCK			0
#define B_LAZY_LOCK			1
#define B_FULL_LOCK			2
#define B_CONTIGUOUS		3
#define B_LOMEM				4
#define B_32_BIT_FULL_LOCK	5
#define B_32_BIT_CONTIGUOUS	6

#define B_INFINITE_TIMEOUT	(0x7FFFFFFFFFFFFFFFLL)

enum {
	B_TIMEOUT					= 0x08,
	B_RELATIVE_TIMEOUT			= 0x08,
	B_ABSOLUTE_TIMEOUT			= 0x10,
	B_TIMEOUT_REAL_TIME_BASE	= 0x40,
	B_ABSOLUTE_REAL_TIME_TIMEOUT = B_ABSOLUTE_TIMEOUT | B_TIMEOUT_REAL_TIME_BASE
};

enum {
	B_DO_NOT_RESCHEDULE	= 0x02,
	B_RELEASE_ALL		= 0x08
};

area_id		create_area(const char *name, void **startAddress,
				uint32 addressSpec, size_t size, uint32 lock,
				uint32 protection);
area_id		clone_area(const char *name, void **destAddress,
				uint32 addressSpec, uint32 protection, area_id source);
status_t	delete_area(area_id id);
status_t	resize_area(area_id id, size_t newSize);
area_id		area_for(void *address);

status_t	acquire_sem_etc(sem_id id, int32 count, uint32 flags,
				bigtime_t timeout);
status_t	release_sem_etc(sem_id id, int32 count, uint32 flags);
status_t	snooze(bigtime_t amount);

#ifdef __cplusplus
}
#endif

#endif	/* _OS_H */
