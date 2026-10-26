/*
 * PCI.h - Host-only stand-in for Haiku's <PCI.h> (subset used by the driver).
 */
#ifndef _PCI_H
#define _PCI_H

#include <SupportDefs.h>
#include <module.h>

#ifdef __cplusplus
extern "C" {
#endif

#define B_PCI_MODULE_NAME	"bus_managers/pci/v1"

#define PCI_command			0x04
#define PCI_command_io		0x001
#define PCI_command_memory	0x002
#define PCI_command_master	0x004
#define PCI_command_snoop	0x008

#define PCI_interrupt_line	0x3c
#define PCI_interrupt_pin	0x3d

#define PCI_display			0x03

typedef struct bus_manager_info {
	module_info	info;
	status_t	(*rescan)(void);
} bus_manager_info;

typedef struct pci_info {
	ushort	vendor_id;
	ushort	device_id;
	uchar	bus;
	uchar	device;
	uchar	function;
	uchar	revision;
	uchar	class_api;
	uchar	class_sub;
	uchar	class_base;
	uchar	line_size;
	uchar	latency;
	uchar	header_type;
	uchar	bist;
	uchar	reserved;
	union {
		struct {
			uint32	cardbus_cis;
			ushort	subsystem_id;
			ushort	subsystem_vendor_id;
			uint32	rom_base;
			uint32	rom_base_pci;
			uint32	rom_size;
			uint32	base_registers[6];
			uint32	base_registers_pci[6];
			uint32	base_register_sizes[6];
			uchar	base_register_flags[6];
			uchar	interrupt_line;
			uchar	interrupt_pin;
			uchar	min_grant;
			uchar	max_latency;
		} h0;
	} u;
} pci_info;

typedef struct pci_module_info pci_module_info;

struct pci_module_info {
	bus_manager_info	binfo;

	uint8	(*read_io_8)(int mapped_io_addr);
	void	(*write_io_8)(int mapped_io_addr, uint8 value);
	uint16	(*read_io_16)(int mapped_io_addr);
	void	(*write_io_16)(int mapped_io_addr, uint16 value);
	uint32	(*read_io_32)(int mapped_io_addr);
	void	(*write_io_32)(int mapped_io_addr, uint32 value);

	long	(*get_nth_pci_info)(long index, pci_info *info);
	uint32	(*read_pci_config)(uint8 bus, uint8 device, uint8 function,
				uint16 offset, uint8 size);
	void	(*write_pci_config)(uint8 bus, uint8 device, uint8 function,
				uint16 offset, uint8 size, uint32 value);

	phys_addr_t	(*ram_address)(phys_addr_t physical_address_in_system_memory);

	status_t	(*reserve_device)(uchar bus, uchar device, uchar function,
				const char *driver_name, void *cookie);
	status_t	(*unreserve_device)(uchar bus, uchar device, uchar function,
				const char *driver_name, void *cookie);
};

#ifdef __cplusplus
}
#endif

#endif	/* _PCI_H */
