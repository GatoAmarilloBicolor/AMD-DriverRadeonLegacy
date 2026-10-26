/*
 * vram_compression.cpp - VRAM surface compression + dynamic 32→16 bit quantization
 * Target: pre-R600 / R600 GPUs with ≤512 MB VRAM
 */
#include "vram_compression.h"
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

/* ---------- LZ4 fast path (stub; replace with real LZ4 if available) ---------- */
static bool lz4_compress(const void *src, size_t src_size, void *dst, size_t *dst_size)
{
	if (src_size < 64) return false;
	memcpy(dst, src, src_size);
	*dst_size = src_size;
	return true;
}
static bool lz4_decompress(const void *src, size_t src_size, void *dst, size_t dst_size)
{
	memcpy(dst, src, src_size);
	return true;
}

/* ---------- Delta Color Compression (simplified 2x2 block delta) ------------- */
static bool dcc_compress(const uint32_t *src, size_t pixels, uint32_t *dst, size_t *out_size)
{
	if (pixels < 4) return false;
	size_t blocks = pixels / 4;
	for (size_t i = 0; i < blocks; ++i) {
		uint32_t base = src[i * 4];
		dst[i * 5] = base;
		for (int c = 0; c < 4; ++c) {
			uint32_t p = src[i * 4 + c];
			int dr = (int)((p >> 16) & 0xFF) - (int)((base >> 16) & 0xFF);
			int dg = (int)((p >> 8) & 0xFF) - (int)((base >> 8) & 0xFF);
			int db = (int)(p & 0xFF) - (int)(base & 0xFF);
			dst[i * 5 + 1 + c] = ((dr & 0x1F) << 10) | ((dg & 0x1F) << 5) | (db & 0x1F);
		}
	}
	*out_size = blocks * 5 * sizeof(uint32_t);
	return true;
}
static bool dcc_decompress(const uint32_t *src, size_t blocks, uint32_t *dst)
{
	for (size_t i = 0; i < blocks; ++i) {
		uint32_t base = src[i * 5];
		for (int c = 0; c < 4; ++c) {
			uint32_t d = src[i * 5 + 1 + c];
			int dr = (int)(d >> 10) & 0x1F; if (dr & 0x10) dr -= 0x20;
			int dg = (int)(d >> 5) & 0x1F;  if (dg & 0x10) dg -= 0x20;
			int db = (int)d & 0x1F;          if (db & 0x10) db -= 0x20;
			uint32_t r = ((base >> 16) & 0xFF) + dr;
			uint32_t g = ((base >> 8) & 0xFF) + dg;
			uint32_t b = (base & 0xFF) + db;
			dst[i * 4 + c] = (r << 16) | (g << 8) | b | 0xFF000000;
		}
	}
	return true;
}

/* ---------- 32-bit RGBA8 → 16-bit RGB565 / 8-bit Luminance quantization ----- */
bool vram_quantize_texture_to_16bit(const uint32_t *src_rgba8, uint16_t *dst_rgb565, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; ++i) {
		uint32_t p = src_rgba8[i];
		uint16_t r = (p >> 19) & 0x1F;        // 8→5
		uint16_t g = (p >> 10) & 0x3F;        // 8→6
		uint16_t b = (p >> 3) & 0x1F;         // 8→5
		dst_rgb565[i] = (r << 11) | (g << 5) | b;
	}
	return true;
}

bool vram_quantize_texture_to_8bit(const uint32_t *src_rgba8, uint8_t *dst_l8, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; ++i) {
		uint32_t p = src_rgba8[i];
		uint32_t r = (p >> 16) & 0xFF;
		uint32_t g = (p >> 8) & 0xFF;
		uint32_t b = p & 0xFF;
		// Coeficientes estándar de luminancia ITU-R BT.601
		dst_l8[i] = (uint8_t)((r * 299 + g * 587 + b * 114) / 1000);
	}
	return true;
}

static bool vram_dequantize_16bit_to_32bit(const uint16_t *src_rgb565, uint32_t *dst_rgba8, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; ++i) {
		uint16_t p = src_rgb565[i];
		uint32_t r = ((p >> 11) & 0x1F) << 3;  // 5→8
		uint32_t g = ((p >> 5) & 0x3F) << 2;   // 6→8
		uint32_t b = (p & 0x1F) << 3;          // 5→8
		dst_rgba8[i] = (r << 16) | (g << 8) | b | 0xFF000000;
	}
	return true;
}

static bool vram_dequantize_8bit_to_32bit(const uint8_t *src_l8, uint32_t *dst_rgba8, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; ++i) {
		uint32_t v = src_l8[i];
		dst_rgba8[i] = (v << 16) | (v << 8) | v | 0xFF000000;
	}
	return true;
}

/* ---------- Depth / Stencil Reduction (D24S8 / D32F → 16-bit Depth) ----------- */
bool vram_reduce_depth_surface(const uint32_t *src_depth32, uint16_t *dst_depth16, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; ++i) {
		uint32_t val = src_depth32[i];
		dst_depth16[i] = (uint16_t)(val >> (val > 0x00FFFFFF ? 16 : 8));
	}
	return true;
}

static bool vram_expand_depth_surface(const uint16_t *src_depth16, uint32_t *dst_depth32, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; ++i) {
		uint32_t val = src_depth16[i];
		dst_depth32[i] = val << 8;
	}
	return true;
}

/* ---------- VRAM Pressure Observer Implementation ----------------------------- */
class VramCompressionObserver : public IVramPressureObserver {
public:
    VramCompressionObserver() : fSeverePressure(false) {}
    
    void on_pressure_changed(bool severe_pressure) override {
        fSeverePressure = severe_pressure;
    }
    
    bool is_severe_pressure() const { return fSeverePressure; }
    
private:
    bool fSeverePressure;
};

static VramCompressionObserver gVramCompressionObserver;

/* C-compatible function for vram_compress_surface to check pressure */
extern "C" bool vram_is_under_severe_pressure(void) {
    return gVramCompressionObserver.is_severe_pressure();
}

/* Public API to register observer with VramPressureMonitor */
void vram_register_pressure_observer(VramPressureMonitor *monitor) {
    if (monitor) {
        monitor->add_observer(&gVramCompressionObserver);
    }
}

/* ---------- Public API ------------------------------------------------------ */
bool vram_compress_surface(struct vram_surface *surface)
{
	if (!surface || !surface->data || surface->size_bytes == 0) return false;
	size_t pixel_count = surface->size_bytes / 4;

	if (vram_is_under_severe_pressure()) {
		if (surface->mode == VRAM_COMPRESS_DEPTH_REDUCE) {
			uint16_t *dst16 = (uint16_t *)malloc(pixel_count * 2);
			if (!dst16) return false;
			vram_reduce_depth_surface((const uint32_t *)surface->data, dst16, pixel_count);
			free(surface->data);
			surface->data = dst16;
			surface->size_bytes = pixel_count * 2;
			surface->is_compressed = true;
			return true;
		}
		if (surface->mode == VRAM_COMPRESS_QUANTIZE_16BIT || surface->size_bytes > 4096 * 1024) {
			uint8_t *dst8 = (uint8_t *)malloc(pixel_count);
			if (!dst8) return false;
			vram_quantize_texture_to_8bit((const uint32_t *)surface->data, dst8, pixel_count);
			free(surface->data);
			surface->data = dst8;
			surface->size_bytes = pixel_count;
			surface->mode = VRAM_COMPRESS_QUANTIZE_8BIT;
			surface->is_compressed = true;
			return true;
		} else {
			uint16_t *dst16 = (uint16_t *)malloc(pixel_count * 2);
			if (!dst16) return false;
			vram_quantize_texture_to_16bit((const uint32_t *)surface->data, dst16, pixel_count);
			free(surface->data);
			surface->data = dst16;
			surface->size_bytes = pixel_count * 2;
			surface->mode = VRAM_COMPRESS_QUANTIZE_16BIT;
			surface->is_compressed = true;
			return true;
		}
	}

	if (surface->mode == VRAM_COMPRESS_FAST_LZ4) {
		void *dst = malloc(surface->size_bytes);
		size_t dst_size;
		if (!lz4_compress(surface->data, surface->size_bytes, dst, &dst_size)) {
			free(dst); return false;
		}
		free(surface->data);
		surface->data = dst;
		surface->size_bytes = dst_size;
		surface->is_compressed = true;
		return true;
	}

	if (surface->mode == VRAM_COMPRESS_DELTA_COLOR) {
		uint32_t *dst = (uint32_t *)malloc(pixel_count * 5 * sizeof(uint32_t));
		size_t dst_size;
		if (!dcc_compress((const uint32_t *)surface->data, pixel_count, dst, &dst_size)) {
			free(dst); return false;
		}
		free(surface->data);
		surface->data = dst;
		surface->size_bytes = dst_size;
		surface->is_compressed = true;
		return true;
	}
	return false;
}

bool vram_decompress_surface(struct vram_surface *surface)
{
	if (!surface || !surface->data || !surface->is_compressed) return false;
	size_t pixel_count = (surface->mode == VRAM_COMPRESS_QUANTIZE_8BIT) ? surface->size_bytes : (surface->size_bytes / 4);

	switch (surface->mode) {
	case VRAM_COMPRESS_DEPTH_REDUCE: {
		uint32_t *dst32 = (uint32_t *)malloc(pixel_count * 4);
		if (!dst32) return false;
		vram_expand_depth_surface((const uint16_t *)surface->data, dst32, pixel_count);
		free(surface->data);
		surface->data = dst32;
		surface->size_bytes = pixel_count * 4;
		surface->is_compressed = false;
		return true;
	}
	case VRAM_COMPRESS_QUANTIZE_8BIT: {
		uint32_t *dst32 = (uint32_t *)malloc(pixel_count * 4);
		if (!dst32) return false;
		vram_dequantize_8bit_to_32bit((const uint8_t *)surface->data, dst32, pixel_count);
		free(surface->data);
		surface->data = dst32;
		surface->size_bytes = pixel_count * 4;
		surface->is_compressed = false;
		return true;
	}
	case VRAM_COMPRESS_QUANTIZE_16BIT: {
		uint32_t *dst32 = (uint32_t *)malloc(pixel_count * 4);
		if (!dst32) return false;
		vram_dequantize_16bit_to_32bit((const uint16_t *)surface->data, dst32, pixel_count);
		free(surface->data);
		surface->data = dst32;
		surface->size_bytes = pixel_count * 4;
		surface->is_compressed = false;
		return true;
	}
	case VRAM_COMPRESS_FAST_LZ4: {
		void *dst = malloc(pixel_count * 4);
		if (!lz4_decompress(surface->data, surface->size_bytes, dst, pixel_count * 4)) {
			free(dst); return false;
		}
		free(surface->data);
		surface->data = dst;
		surface->size_bytes = pixel_count * 4;
		surface->is_compressed = false;
		return true;
	}
	case VRAM_COMPRESS_DELTA_COLOR: {
		size_t blocks = pixel_count / 4;
		uint32_t *dst = (uint32_t *)malloc(pixel_count * 4);
		if (!dcc_decompress((const uint32_t *)surface->data, blocks, dst)) {
			free(dst); return false;
		}
		free(surface->data);
		surface->data = dst;
		surface->size_bytes = pixel_count * 4;
		surface->is_compressed = false;
		return true;
	}
	default:
		return false;
	}
}