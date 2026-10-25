/*
 * vram_compression.h - Compresión de superficies y texturas en VRAM (DCC / HTM / Z-Compression simulado)
 * Diseñado para mitigar la saturación del bus PCIe en GPUs legacy con VRAM minúscula.
 */
#ifndef VRAM_COMPRESSION_H
#define VRAM_COMPRESSION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum vram_compression_mode {
	VRAM_COMPRESS_NONE = 0,
	VRAM_COMPRESS_FAST_LZ4 = 1,
	VRAM_COMPRESS_DELTA_COLOR = 2
};

struct vram_surface {
	void *data;
	size_t size_bytes;
	enum vram_compression_mode mode;
	bool is_compressed;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Comprime una superficie o textura antes de transferirla a VRAM o GTT */
bool vram_compress_surface(struct vram_surface *surface);

/* Descomprime una superficie al ser solicitada por el renderizador o la CPU */
bool vram_decompress_surface(struct vram_surface *surface);

#ifdef __cplusplus
}
#endif

#endif /* VRAM_COMPRESSION_H */
