/*
 * haiku_wsi.c - Implementación de la WSI nativa para Haiku (VK_HAIKU_bmessenger_surface)
 */
#include "haiku_wsi.h"
#include <stdlib.h>
#include <string.h>

static uint32_t g_surface_counter = 0;

int32_t haiku_create_bmessenger_surface(
	const struct haiku_surface_create_info *pCreateInfo,
	uint32_t *pSurface)
{
	if (pCreateInfo == NULL || pSurface == NULL)
		return -1;
	g_surface_counter++;
	*pSurface = g_surface_counter;
	return 0;
}

int32_t haiku_surface_present(uint32_t surface_id, uint32_t bo_handle, void *host_ptr, uint32_t width, uint32_t height)
{
	if (surface_id == 0 || host_ptr == NULL || width == 0 || height == 0)
		return -1;
	(void)bo_handle;
	/* Presentación simulada / copia hacia BBitmap compartido con app_server */
	return 0;
}
