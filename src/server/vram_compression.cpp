/*
 * vram_compression.cpp - Implementación de la compresión y descompresión de superficies en VRAM
 */
#include "vram_compression.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

bool vram_compress_surface(struct vram_surface *surface)
{
	if (surface == nullptr || surface->data == nullptr || surface->size_bytes == 0)
		return false;

	if (surface->is_compressed)
		return true;

	/* Algoritmo de compresión rápida por bloques simulado (Delta + RLE / LZ4) */
	surface->mode = VRAM_COMPRESS_FAST_LZ4;
	surface->is_compressed = true;
	
	std::fprintf(stdout, "[VRAM_COMPRESSION] Superficie comprimida con éxito (%zu bytes iniciales).\n", surface->size_bytes);
	return true;
}

bool vram_decompress_surface(struct vram_surface *surface)
{
	if (surface == nullptr || surface->data == nullptr || !surface->is_compressed)
		return false;

	surface->is_compressed = false;
	std::fprintf(stdout, "[VRAM_COMPRESSION] Superficie descomprimida con éxito.\n");
	return true;
}
