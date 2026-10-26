/*
 * radeon_legacy_driver.cpp - Add-on gráfico nativo de Haiku OS para Radeon Legacy
 * Conecta el app_server con el demonio radeon_gpud y el helper de kernel.
 * Implementa hooks requeridos por Accelerant (GraphicsCard.h / Accelerant.h).
 */
#include <GraphicsCard.h>
#include <Accelerant.h>
#include <SupportDefs.h>
#include <OS.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "haiku_kits_integration.h"
#include "RadeonAddonIntegration.h"

/* ------------------------------------------------------------------ */
/* Estado global del acelerador                                        */
/* ------------------------------------------------------------------ */

#define MAX_HEADS 4

typedef struct radeon_head_state {
    uint32_t head_id;
    bool active;
    display_mode current_mode;
    area_id framebuffer_area;
    void *framebuffer_virt;
    uint32_t crtc_index;
} radeon_head_state;

typedef struct radeon_accelerant_state {
    int fd;                      /* File descriptor del helper (/dev/graphics/radeon_hd) */
    area_id regs_area;           /* Área MMIO clonada desde helper */
    void *regs_virt;             /* Puntero virtual a registros MMIO */
    uint32_t head_count;         /* Número de pantallas detectadas (Multi-Head) */
    radeon_head_state heads[MAX_HEADS];
} radeon_accelerant_state;

static radeon_accelerant_state *g_state = NULL;

/* ------------------------------------------------------------------ */
/* Helpers internos                                                    */
/* ------------------------------------------------------------------ */

static status_t open_helper_device(void)
{
    int fd = open("/dev/graphics/radeon_hd", B_READ_WRITE);
    if (fd < 0) {
        fd = open("/dev/graphics/radeon_legacy", B_READ_WRITE); /* standalone */
    }
    return fd;
}

static void close_helper_device(int fd)
{
    if (fd >= 0)
        close(fd);
}

/* Clona área del helper al espacio del app_server */
static status_t clone_helper_area(int helper_fd, const char *area_name,
                                  uint32_t ioctl_code, area_id *out_area, void **out_virt)
{
    rg_map_request req = { 0 };
    req.flags = RG_MAP_READ | RG_MAP_WRITE;
    
    if (ioctl(helper_fd, ioctl_code, &req, sizeof(req)) != B_OK)
        return B_ERROR;
    
    if (req.area < 0)
        return B_ERROR;
    
    *out_area = clone_area(area_name, out_virt, B_ANY_ADDRESS,
                           B_READ_AREA | B_WRITE_AREA, req.area);
    if (*out_area < 0)
        return *out_area;
    
    return B_OK;
}

/* ------------------------------------------------------------------ */
/* Hooks requeridos por Accelerant                                     */
/* ------------------------------------------------------------------ */

status_t init_hardware(void)
{
    g_state = (radeon_accelerant_state *)calloc(1, sizeof(radeon_accelerant_state));
    if (!g_state)
        return B_NO_MEMORY;
    
    g_state->fd = open_helper_device();
    if (g_state->fd < 0) {
        free(g_state);
        g_state = NULL;
        return B_ERROR;
    }
    
    g_state->head_count = 2; /* Soporte multi-head estándar (CRTC0 y CRTC1) */
    for (uint32_t i = 0; i < g_state->head_count; i++) {
        g_state->heads[i].head_id = i;
        g_state->heads[i].active = (i == 0); /* Head principal activo por defecto */
        g_state->heads[i].crtc_index = i;
        g_state->heads[i].framebuffer_area = -1;
        g_state->heads[i].framebuffer_virt = NULL;
    }
    
    return B_OK;
}

void uninit_hardware(void)
{
    if (!g_state)
        return;
    
    if (g_state->regs_area >= 0) {
        delete_area(g_state->regs_area);
        g_state->regs_area = -1;
    }
    for (uint32_t i = 0; i < g_state->head_count; i++) {
        if (g_state->heads[i].framebuffer_area >= 0) {
            delete_area(g_state->heads[i].framebuffer_area);
            g_state->heads[i].framebuffer_area = -1;
        }
    }
    close_helper_device(g_state->fd);
    free(g_state);
    g_state = NULL;
}

status_t get_device_info(device_info *info)
{
    if (!info)
        return B_BAD_VALUE;
    
    if (!g_state)
        return B_NO_INIT;
    
    rg_kernel_info kinfo = { 0 };
    if (ioctl(g_state->fd, RG_IOCTL_GET_INFO, &kinfo, sizeof(kinfo)) != B_OK)
        return B_ERROR;
    
    info->vendor_id = kinfo.vendor_id;
    info->device_id = kinfo.device_id;
    info->revision = kinfo.revision;
    info->flags = 0;
    if (kinfo.host_coherent)
        info->flags |= B_HARDWARE_CURSOR;
    
    return B_OK;
}

status_t get_mode_list(display_mode *modes, uint32_t *count)
{
    if (!modes || !count)
        return B_BAD_VALUE;
    
    /* Modos básicos soportados - en producción leer de EDID */
    static const display_mode supported_modes[] = {
        { 640, 480, 600000, 0, B_CMAP8 },
        { 800, 600, 600000, 0, B_CMAP8 },
        { 1024, 768, 600000, 0, B_CMAP8 },
        { 1280, 1024, 600000, 0, B_CMAP8 },
        { 1920, 1080, 600000, 0, B_CMAP8 },
        { 640, 480, 600000, 0, B_RGB15 },
        { 800, 600, 600000, 0, B_RGB15 },
        { 1024, 768, 600000, 0, B_RGB15 },
        { 1280, 1024, 600000, 0, B_RGB15 },
        { 1920, 1080, 600000, 0, B_RGB15 },
        { 640, 480, 600000, 0, B_RGB16 },
        { 800, 600, 600000, 0, B_RGB16 },
        { 1024, 768, 600000, 0, B_RGB16 },
        { 1280, 1024, 600000, 0, B_RGB16 },
        { 1920, 1080, 600000, 0, B_RGB16 },
        { 640, 480, 600000, 0, B_RGB32 },
        { 800, 600, 600000, 0, B_RGB32 },
        { 1024, 768, 600000, 0, B_RGB32 },
        { 1280, 1024, 600000, 0, B_RGB32 },
        { 1920, 1080, 600000, 0, B_RGB32 },
    };
    
    *count = sizeof(supported_modes) / sizeof(supported_modes[0]);
    if (modes)
        memcpy(modes, supported_modes, sizeof(supported_modes));
    
    return B_OK;
}

status_t proposed_mode(display_mode *mode)
{
    if (!mode)
        return B_BAD_VALUE;
    
    /* Validar modo contra lista conocida */
    uint32_t count = 0;
    get_mode_list(NULL, &count);
    display_mode *list = (display_mode *)malloc(count * sizeof(display_mode));
    if (!list)
        return B_NO_MEMORY;
    
    get_mode_list(list, &count);
    
    for (uint32_t i = 0; i < count; i++) {
        if (list[i].virtual_width == mode->virtual_width &&
            list[i].virtual_height == mode->virtual_height &&
            list[i].space == mode->space) {
            /* Ajustar timing exacto */
            *mode = list[i];
            free(list);
            return B_OK;
        }
    }
    
    free(list);
    return B_BAD_VALUE;
}

status_t set_mode(display_mode *mode)
{
    if (!g_state || !mode)
        return B_BAD_VALUE;
    
    /* Validar modo */
    if (proposed_mode(mode) != B_OK)
        return B_BAD_VALUE;
    
    /* Mapear MMIO si no está mapeado */
    if (g_state->regs_area < 0) {
        status_t st = clone_helper_area(g_state->fd, "radeon_mmio",
                                        RG_IOCTL_MAP_MMIO,
                                        &g_state->regs_area, &g_state->regs_virt);
        if (st != B_OK)
            return st;
    }
    
    /* Mapear framebuffer (VRAM) */
    if (g_state->framebuffer_area < 0) {
        status_t st = clone_helper_area(g_state->fd, "radeon_fb",
                                        RG_IOCTL_MAP_VRAM,
                                        &g_state->framebuffer_area, &g_state->framebuffer_virt);
        if (st != B_OK)
            return st;
    }
    
    /* Configurar modo en hardware - simplificado */
    rg_kernel_info kinfo = { 0 };
    if (ioctl(g_state->fd, RG_IOCTL_GET_INFO, &kinfo, sizeof(kinfo)) != B_OK)
        return B_ERROR;
    
    g_state->width = mode->virtual_width;
    g_state->height = mode->virtual_height;
    g_state->bpp = (mode->space == B_CMAP8) ? 8 :
                   (mode->space == B_RGB15 || mode->space == B_RGB16) ? 16 : 32;
    g_state->current_mode = *mode;
    g_state->accelerated = true;
    
    return B_OK;
}

status_t get_mode(display_mode *mode)
{
    if (!g_state || !mode)
        return B_BAD_VALUE;
    
    *mode = g_state->current_mode;
    return B_OK;
}

status_t get_frame_buffer_config(frame_buffer_config *config)
{
    if (!g_state || !config)
        return B_BAD_VALUE;
    
    config->frame_buffer = g_state->framebuffer_virt;
    config->frame_buffer_bytes_per_row = g_state->width * (g_state->bpp / 8);
    config->frame_buffer_depth = g_state->bpp;
    
    return B_OK;
}

/* ------------------------------------------------------------------ */
/* Hooks de aceleración 2D (Engine)                                    */
/* ------------------------------------------------------------------ */

uint32_t acceleration_flags(void)
{
    if (!g_state || !g_state->accelerated)
        return 0;
    
    return B_HARDWARE_CURSOR | B_2D_ACCELERATION;
}

status_t fill_rectangle(uint32 color, fill_rect_params *params, uint32 count)
{
    if (!g_state || !params || count == 0)
        return B_BAD_VALUE;
    
    if (!g_state->accelerated)
        return B_ERROR;
    
    /* Usar ASM blit si disponible, fallback CPU */
    for (uint32 i = 0; i < count; i++) {
        haiku_app_kit_blit_surface(NULL, g_state->framebuffer_virt,
                                   params[i].right - params[i].left + 1,
                                   params[i].bottom - params[i].top + 1);
    }
    return B_OK;
}

status_t invert_rectangle(fill_rect_params *params, uint32 count)
{
    return B_ERROR; /* No implementado */
}

status_t blit(blit_params *params, uint32 count)
{
    if (!g_state || !params || count == 0)
        return B_BAD_VALUE;
    
    if (!g_state->accelerated)
        return B_ERROR;
    
    for (uint32 i = 0; i < count; i++) {
        haiku_app_kit_blit_surface(
            (const void *)params[i].src_offset,
            g_state->framebuffer_virt,
            params[i].width,
            params[i].height
        );
    }
    return B_OK;
}

status_t span_fill(uint32 color, span_params *params, uint32 count)
{
    return B_ERROR; /* No implementado */
}

status_t screen_to_screen_blit(blit_params *params, uint32 count)
{
    return blit(params, count);
}

/* ------------------------------------------------------------------ */
/* Cursor de hardware                                                  */
/* ------------------------------------------------------------------ */

status_t set_cursor_shape(cursor_shape *shape)
{
    if (!g_state || !g_state->accelerated)
        return B_ERROR;
    return B_OK; /* Implementación simplificada */
}

status_t move_cursor(uint16 x, uint16 y)
{
    if (!g_state || !g_state->accelerated)
        return B_ERROR;
    return B_OK; /* Implementación simplificada */
}

status_t show_cursor(bool visible)
{
    if (!g_state || !g_state->accelerated)
        return B_ERROR;
    return B_OK; /* Implementación simplificada */
}

/* ------------------------------------------------------------------ */
/* Sync / IRQ                                                          */
/* ------------------------------------------------------------------ */

status_t sync_token(uint32 *token)
{
    if (!token)
        return B_BAD_VALUE;
    *token = 0; /* Simplificado */
    return B_OK;
}

status_t sync_to_token(uint32 token)
{
    (void)token;
    return B_OK; /* Simplificado */
}

/* ------------------------------------------------------------------ */
/* Engine control                                                      */
/* ------------------------------------------------------------------ */

engine_token *acquire_engine(uint32 capabilities, uint32 max_wait, sync_token *st)
{
    (void)capabilities; (void)max_wait; (void)st;
    return NULL; /* No implementado */
}

status_t release_engine(engine_token *et)
{
    (void)et;
    return B_OK;
}

/* ------------------------------------------------------------------ */
/* Entry points obligatorios                                           */
/* ------------------------------------------------------------------ */

status_t radeon_init_accelerant(int device_fd)
{
    (void)device_fd;
    return init_hardware();
}

void radeon_uninit_accelerant(void)
{
    uninit_hardware();
}

status_t init_accelerant(device_info *info)
{
    register_replicant();
    return get_device_info(info);
}

void uninit_accelerant(void)
{
    uninit_hardware();
}

status_t accelerant_mode_count(uint32 *count)
{
    return get_mode_list(NULL, count);
}

status_t accelerant_get_mode_list(display_mode *modes, uint32 *count)
{
    return get_mode_list(modes, count);
}

status_t accelerant_propose_mode(display_mode *mode)
{
    return proposed_mode(mode);
}

status_t accelerant_set_mode(display_mode *mode)
{
    return set_mode(mode);
}

status_t accelerant_get_mode(display_mode *mode)
{
    return get_mode(mode);
}

status_t accelerant_get_frame_buffer_config(frame_buffer_config *config)
{
    return get_frame_buffer_config(config);
}

uint32 accelerant_engine_count(void)
{
    return 1; /* Un motor 2D */
}

status_t accelerant_acquire_engine(uint32 capabilities, uint32 max_wait, sync_token *st)
{
    return acquire_engine(capabilities, max_wait, st) ? B_OK : B_ERROR;
}

status_t accelerant_release_engine(engine_token *et)
{
    return release_engine(et);
}

uint32 accelerant_get_engine_token(void)
{
    return 0;
}

status_t accelerant_set_cursor_shape(cursor_shape *shape)
{
    return set_cursor_shape(shape);
}

status_t accelerant_move_cursor(uint16 x, uint16 y)
{
    return move_cursor(x, y);
}

status_t accelerant_show_cursor(bool visible)
{
    return show_cursor(visible);
}

status_t accelerant_sync_token(uint32 *token)
{
    return sync_token(token);
}

status_t accelerant_sync_to_token(uint32 token)
{
    return sync_to_token(token);
}

/* ------------------------------------------------------------------ */
/* Funciones 2D Engine                                                 */
/* ------------------------------------------------------------------ */

status_t accelerant_fill_rectangle(uint32 color, fill_rect_params *params, uint32 count)
{
    return fill_rectangle(color, params, count);
}

status_t accelerant_invert_rectangle(fill_rect_params *params, uint32 count)
{
    return invert_rectangle(params, count);
}

status_t accelerant_blit(blit_params *params, uint32 count)
{
    return blit(params, count);
}

status_t accelerant_span_fill(uint32 color, span_params *params, uint32 count)
{
    return span_fill(color, params, count);
}

status_t accelerant_screen_to_screen_blit(blit_params *params, uint32 count)
{
    return screen_to_screen_blit(params, count);
}

/* Hooks opcionales (no implementados) */

status_t accelerant_engine_restart(void)
{
    return B_OK;
}

status_t accelerant_mode_changed(display_mode *mode)
{
    return B_OK;
}

status_t accelerant_dpms_capabilities(uint32 *capabilities)
{
    if (capabilities)
        *capabilities = B_DPMS_ON | B_DPMS_STAND_BY | B_DPMS_SUSPEND | B_DPMS_OFF;
    return B_OK;
}

status_t accelerant_dpms_mode(uint32 *mode)
{
    if (mode)
        *mode = B_DPMS_ON;
    return B_OK;
}

status_t accelerant_set_dpms_mode(uint32 mode)
{
    (void)mode;
    return B_OK;
}

status_t accelerant_get_pci_info(pci_info *info)
{
    if (!g_state || !info)
        return B_BAD_VALUE;
    
    rg_kernel_info kinfo = { 0 };
    if (ioctl(g_state->fd, RG_IOCTL_GET_INFO, &kinfo, sizeof(kinfo)) != B_OK)
        return B_ERROR;
    
    info->vendor_id = kinfo.vendor_id;
    info->device_id = kinfo.device_id;
    info->revision = kinfo.revision;
    info->bus = kinfo.pci_bus;
    info->device = kinfo.pci_device;
    info->function = kinfo.pci_function;
    return B_OK;
}