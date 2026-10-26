/*
 * firmware.cpp - Implementacion del cargador de firmware.
 */
#include "firmware.h"

#include <cstdio>
#include <cstdlib>

FirmwareLoader::FirmwareLoader(std::string base_dir)
{
	if (!base_dir.empty()) {
		fBaseDir = std::move(base_dir);
		return;
	}
	const char *env = getenv("RG_FIRMWARE_DIR");
	if (env != nullptr && *env != '\0')
		fBaseDir = env;
	else
		fBaseDir = "/boot/system/data/firmware/radeon";
}

std::vector<std::string> FirmwareLoader::manifest_for(rg_family family)
{
	/* Nombres provisionales; confirmar contra linux-firmware/radeon. */
	switch (family) {
		case RG_FAMILY_R300:
			return { "R300_cp.bin" };
		case RG_FAMILY_R600:
			return { "r600_pfp.bin", "r600_me.bin", "r600_rlc.bin" };
		case RG_FAMILY_EVERGREEN:
			return { "cypress_pfp.bin", "cypress_me.bin", "cypress_rlc.bin" };
		case RG_FAMILY_NI:
			return { "cayman_pfp.bin", "cayman_me.bin", "cayman_rlc.bin",
				"cayman_mc.bin" };
		case RG_FAMILY_GCN1:
		case RG_FAMILY_GCN2:
		case RG_FAMILY_GCN3:
			return { "polaris10_pfp.bin", "polaris10_me.bin",
				"polaris10_rlc.bin", "polaris10_mc.bin" };
		case RG_FAMILY_GCN5:
			return { "vega10_pfp.bin", "vega10_me.bin", "vega10_rlc.bin" };
		case RG_FAMILY_RDNA1:
		case RG_FAMILY_RDNA2:
		case RG_FAMILY_RDNA3:
		case RG_FAMILY_RDNA4:
			return { "psp_13_0_0.bin", "navi10_pfp.bin", "navi10_me.bin",
				"navi10_rlc.bin" };
		default:
			return {};
	}
}

int32_t FirmwareLoader::load(const std::string &name, rg_fw_blob *out)
{
	if (out == nullptr)
		return RG_EINVAL;
	std::string path = fBaseDir + "/" + name;
	FILE *f = fopen(path.c_str(), "rb");
	if (f == nullptr)
		return RG_ENOENT;
	if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return RG_EINVAL; }
	long sz = ftell(f);
	if (sz <= 0) { fclose(f); return RG_EINVAL; }
	rewind(f);
	std::vector<uint8_t> buf((size_t)sz);
	size_t rd = fread(buf.data(), 1, buf.size(), f);
	fclose(f);
	if (rd != buf.size())
		return RG_EINVAL;
	out->name = name;
	out->data = std::move(buf);
	return RG_OK;
}

int32_t FirmwareLoader::load_for(rg_family family, std::vector<rg_fw_blob> *out,
	std::vector<std::string> *missing)
{
	if (out == nullptr)
		return RG_EINVAL;
	out->clear();
	if (missing != nullptr)
		missing->clear();
	int32_t loaded = 0;
	for (const std::string &name : manifest_for(family)) {
		rg_fw_blob blob;
		if (load(name, &blob) == RG_OK) {
			out->push_back(std::move(blob));
			loaded++;
		} else if (missing != nullptr) {
			missing->push_back(name);
		}
	}
	return loaded;
}

#include "firmware_fallback.inc"

bool rg_firmware_load_fallback(uint32_t family, const uint32_t **out_data, size_t *out_size) {
    if (family >= RG_FAMILY_EVERGREEN) {
        *out_data = kEvergreenCpMicrocodeFallback;
        *out_size = sizeof(kEvergreenCpMicrocodeFallback);
        return true;
    } else if (family >= RG_FAMILY_R600) {
        *out_data = kR600CpMicrocodeFallback;
        *out_size = sizeof(kR600CpMicrocodeFallback);
        return true;
    }
    return false;
}
