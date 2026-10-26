/*
 * KernelExport.h - Host-only stand-in for Haiku's <KernelExport.h>.
 */
#ifndef _KERNEL_EXPORT_H
#define _KERNEL_EXPORT_H

#include <SupportDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Area protection / address specification flags. */
#define B_READ_AREA				(1 << 0)
#define B_WRITE_AREA			(1 << 1)
#define B_EXECUTE_AREA			(1 << 2)
#define B_STACK_AREA			(1 << 3)
#define B_KERNEL_READ_AREA		(1 << 4)
#define B_KERNEL_WRITE_AREA		(1 << 5)
#define B_KERNEL_EXECUTE_AREA	(1 << 6)
#define B_KERNEL_STACK_AREA		(1 << 7)
#define B_CLONEABLE_AREA		(1 << 8)

#define B_ANY_ADDRESS			0
#define B_EXACT_ADDRESS			1
#define B_BASE_ADDRESS			2
#define B_CLONE_ADDRESS			3
#define B_ANY_KERNEL_ADDRESS	4
#define B_ANY_KERNEL_BLOCK_ADDRESS 5
#define B_RANDOMIZED_ANY_ADDRESS 6
#define B_RANDOMIZED_BASE_ADDRESS 7

/* Physical memory attributes. */
#define B_UNCACHED_MEMORY		(1 << 28)
#define B_WRITE_COMBINING_MEMORY (2 << 28)
#define B_WRITE_THROUGH_MEMORY	(3 << 28)
#define B_WRITE_PROTECTED_MEMORY (4 << 28)
#define B_WRITE_BACK_MEMORY		(5 << 28)
#define B_MEMORY_TYPE_MASK		(0xf << 28)

typedef struct physical_entry {
	phys_addr_t	address;
	phys_size_t	size;
} physical_entry;

typedef int32 (*interrupt_handler)(void *data);

#define B_UNHANDLED_INTERRUPT	0
#define B_HANDLED_INTERRUPT		1
#define B_INVOKE_SCHEDULER		2

#define B_NO_ENABLE_COUNTER		1
#define B_ENABLE_COUNTER		1

status_t	install_io_interrupt_handler(int32 vector, interrupt_handler handler,
				void *data, uint32 flags);
status_t	remove_io_interrupt_handler(int32 vector, interrupt_handler handler,
				void *data);

area_id		map_physical_memory(const char *name, phys_addr_t physicalAddress,
				size_t size, uint32 flags, uint32 protection,
				void **virtualAddress);
int32		get_memory_map(const void *address, size_t numBytes,
				physical_entry *map, int32 numEntries);
status_t	get_memory_map_etc(team_id team, const void *address, size_t numBytes,
				physical_entry *map, uint32 numEntries);

void		spin(bigtime_t numLoops);
/*
 * On the macOS host <stdio.h> also declares int dprintf(int, ...), which
 * conflicts with Haiku's kernel dprintf. Redirect only for the host check;
 * a real Haiku build keeps the kernel symbol.
 */
#if defined(__APPLE__)
void		rg_stub_dprintf(const char *format, ...)
			__attribute__((format(printf, 1, 2)));
#define dprintf(...) rg_stub_dprintf(__VA_ARGS__)
#else
void		dprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif
void		kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif	/* _KERNEL_EXPORT_H */
