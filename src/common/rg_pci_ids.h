/*
 * rg_pci_ids.h - IDs PCI de las GPU que el driver reconoce.
 *
 * GENERADO por scripts/gen_asic_db.py junto con rg_asic_db.inc.
 * No editar a mano: ejecutar `make asic-db`.
 */
#ifndef RG_PCI_IDS_H
#define RG_PCI_IDS_H

#include <stdint.h>

#define RG_PCI_VENDOR_AMD 0x1002

#define RG_PCI_DEVICE_LIST(X) \
	X(0x4144) \
	X(0x4E48) \
	X(0x6640) \
	X(0x6738) \
	X(0x6798) \
	X(0x67B0) \
	X(0x67DF) \
	X(0x687F) \
	X(0x6898) \
	X(0x68B8) \
	X(0x68BA) \
	X(0x68D8) \
	X(0x68F9) \
	X(0x71C0) \
	X(0x7280) \
	X(0x731F) \
	X(0x73BF) \
	X(0x744C) \
	X(0x9400) \
	X(0x9440) \
	X(0x9442) \
	X(0x94B4) \
	X(0x9501) \
	X(0x954F) \
	X(0x9587) \
	X(0x9598) \
	X(0x9802) \
	X(0x9806) \
	X(0x9830)

#define RG_PCI_ID_CASE(id) case id:

static inline int rg_pci_id_is_supported(uint32_t device_id)
{
	switch (device_id) {
		RG_PCI_DEVICE_LIST(RG_PCI_ID_CASE)
			return 1;
		default:
			return 0;
	}
}

#endif /* RG_PCI_IDS_H */
