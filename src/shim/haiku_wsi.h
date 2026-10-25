/*
 * haiku_wsi.h - Extensión WSI nativa para Haiku (VK_HAIKU_bmessenger_surface)
 * Basado en §8.11 del plan de implementación.
 */
#ifndef HAIKU_WSI_H
#define HAIKU_WSI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* haiku_messenger_t;
typedef void* haiku_bitmap_t;

struct haiku_surface_create_info {
	uint32_t sType;
	const void *pNext;
	uint32_t flags;
	haiku_messenger_t messenger;
	haiku_bitmap_t bitmap;
};

/* Creación de superficie WSI nativa Vulkan en Haiku */
int32_t haiku_create_bmessenger_surface(
	const struct haiku_surface_create_info *pCreateInfo,
	uint32_t *pSurface);

/* Presentación de fotograma BO hacia BBitmap / app_server */
int32_t haiku_surface_present(uint32_t surface_id, uint32_t bo_handle, void *host_ptr, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif /* HAIKU_WSI_H */
