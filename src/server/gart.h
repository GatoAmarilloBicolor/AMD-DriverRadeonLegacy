/*
 * gart.h - Tabla GART (Graphics Address Remapping Table) del servidor.
 *
 * La GPU legacy ve la RAM del sistema a traves de esta tabla. El servidor
 * pide las paginas fisicas al helper (TRANSLATE_PHYS) y escribe una entrada
 * por pagina. Aqui se modela la tabla y se valida; el volcado real a VRAM y
 * el flush de TLB son responsabilidad del backend de familia [HUMANO].
 */
#ifndef RG_GART_H
#define RG_GART_H

#include <stdint.h>
#include <mutex>
#include <vector>

#include "../common/rg_types.h"

struct rg_phys_range {
	uint64_t phys;
	uint64_t size;
};

class GartTable {
public:
	explicit GartTable(uint64_t size, uint64_t gpu_base = 0x80000000ull);

	/* Mapea `size` bytes de RAM fisica (ranges contiguos) en `offset`. */
	int32_t map(uint64_t offset, const rg_phys_range *ranges, uint32_t n_ranges);
	int32_t unmap(uint64_t offset, uint64_t size);

	/* Traduce una direccion GPU de la ventana GTT a fisica. */
	int32_t translate(uint64_t gpu_addr, uint64_t *phys) const;

	uint64_t size() const { return fSize; }
	uint64_t gpu_base() const { return fGpuBase; }
	uint32_t mapped_pages() const;
	/* Contador de invalidaciones/TLB flush solicitados. */
	uint32_t flushes() const { return fFlushes; }
	void invalidate() { fFlushes++; }

private:
	uint32_t page_index(uint64_t offset) const;
	int32_t write_pages(uint64_t offset, uint64_t phys, uint64_t size);

	uint64_t fSize;
	uint64_t fGpuBase;
	std::vector<uint64_t> fEntries; /* 0 = no mapeado */
	mutable std::mutex fLock;
	uint32_t fFlushes = 0;
};

#endif /* RG_GART_H */
