/*
 * vram_compression.h - Compresión de superficies y reducción dinámica de bit-depth (32-bit a 16-bit RGB565) bajo estrés de VRAM
 */
#ifndef VRAM_COMPRESSION_H
#define VRAM_COMPRESSION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum vram_compression_mode {
	VRAM_COMPRESS_NONE = 0,
	VRAM_COMPRESS_FAST_LZ4 = 1,
	VRAM_COMPRESS_DELTA_COLOR = 2,
	VRAM_COMPRESS_QUANTIZE_16BIT = 3 /* Reducción dinámica de 32-bit RGBA8 a 16-bit RGB565 */
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

/* Reduce dinámicamente la indexación de bits de 32-bit (RGBA8) a 16-bit (RGB565) bajo estrés severo de VRAM */
bool vram_quantize_texture_to_16bit(const uint32_t *src_rgba8, uint16_t *dst_rgb565, size_t pixel_count);

#ifdef __cplusplus
}
#endif

#endif /* VRAM_COMPRESSION_H */
