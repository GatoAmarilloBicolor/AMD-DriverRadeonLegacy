/*
 * rg_asic.cpp - Enumerador y base de datos (C8 / §7A.3).
 */
#include "rg_asic.h"

#include <cstring>

#define RG_ASIC_ENTRY(name_, dev_, rev_, sub_, fam_, gfx_, apu_, vram_, feat_) \
	{ name_, dev_, rev_, sub_, fam_, gfx_, apu_, vram_, 0, feat_ }

static const rg_asic_desc kAsicTable[] = {
#include "rg_asic_db.inc"
};

#undef RG_ASIC_ENTRY

const rg_asic_desc rg_asic_unknown = {
	"Unknown Radeon", 0, 0, 0, RG_FAMILY_UNKNOWN, 0, 0, RG_VRAM_UNKNOWN, 0, 0
};

const rg_asic_desc *rg_asic_table(uint32_t *count)
{
	if (count != nullptr)
		*count = (uint32_t)(sizeof(kAsicTable) / sizeof(kAsicTable[0]));
	return kAsicTable;
}

static bool match(const rg_asic_desc &e, uint32_t dev, uint32_t rev, uint32_t sub)
{
	if (e.device_id != dev)
		return false;
	if (e.revision != 0xFFFFFFFF && rev != 0xFFFFFFFF && e.revision != 0)
		if (e.revision != rev)
			return false;
	if (e.subsystem != 0xFFFFFFFF && sub != 0xFFFFFFFF &&
	    e.subsystem != 0 && e.subsystem != sub)
		return false;
	return true;
}

const rg_asic_desc *rg_asic_lookup(uint32_t device_id, uint32_t revision,
	uint32_t subsystem)
{
	/* Primera pasada: coincidencia con revision/subsystem especificos. */
	for (const rg_asic_desc &e : kAsicTable) {
		if (e.device_id != device_id)
			continue;
		if (e.revision == 0xFFFFFFFF && e.subsystem == 0xFFFFFFFF)
			continue;
		if (match(e, device_id, revision, subsystem))
			return &e;
	}
	/* Segunda pasada: entrada generica del device id. */
	for (const rg_asic_desc &e : kAsicTable) {
		if (match(e, device_id, revision, subsystem))
			return &e;
	}
	return &rg_asic_unknown;
}

const char *rg_asic_family_name(rg_family family)
{
	switch (family) {
		case RG_FAMILY_R300: return "r300";
		case RG_FAMILY_R600: return "r600";
		case RG_FAMILY_EVERGREEN: return "evergreen";
		case RG_FAMILY_NI: return "northern-islands";
		case RG_FAMILY_GCN1: return "gcn1";
		case RG_FAMILY_GCN2: return "gcn2";
		case RG_FAMILY_GCN3: return "gcn3";
		case RG_FAMILY_GCN5: return "gcn5";
		case RG_FAMILY_RDNA1: return "rdna1";
		case RG_FAMILY_RDNA2: return "rdna2";
		case RG_FAMILY_RDNA3: return "rdna3";
		case RG_FAMILY_RDNA4: return "rdna4";
		default: return "unknown";
	}
}
