/*
 * firmware.h - Carga de microcodigo (C2 / §6.6).
 *
 * Los nombres exactos de blob dependen del ASIC y deben confirmarse contra
 * linux-firmware/radeon (marcado [VERIFICAR] en el plan). Aqui se declara el
 * manifiesto por familia y un cargador que tolera ausencia (sin microcodigo
 * no hay CP, pero el servidor debe arrancar y reportarlo).
 */
#ifndef RG_FIRMWARE_H
#define RG_FIRMWARE_H

#include <stdint.h>
#include <string>
#include <vector>

#include "../common/rg_types.h"

struct rg_fw_blob {
	std::string name;
	std::vector<uint8_t> data;
};

class FirmwareLoader {
public:
	explicit FirmwareLoader(std::string base_dir = "");

	/* Devuelve lista de nombres esperados para la familia. */
	static std::vector<std::string> manifest_for(rg_family family);

	/* Intenta cargar todos los blobs del manifiesto. Los ausentes se omiten
	 * y se registran en `missing`. Devuelve numero de blobs cargados. */
	int32_t load_for(rg_family family, std::vector<rg_fw_blob> *out,
		std::vector<std::string> *missing);

	/* Carga un blob concreto; RG_ENOENT si no existe. */
	int32_t load(const std::string &name, rg_fw_blob *out);

	std::string base_dir() const { return fBaseDir; }

private:
	std::string fBaseDir;
};

#endif /* RG_FIRMWARE_H */
