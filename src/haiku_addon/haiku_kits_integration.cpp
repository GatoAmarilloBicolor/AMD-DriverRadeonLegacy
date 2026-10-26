/*
 * haiku_kits_integration.cpp - Pasarelas de integración nativa con Media Kit, GL Kit, App Kit y ROCm/ZLUDA
 * Implementaciones stub que conectan con radeon_gpud vía IPC o shims.
 */
#include "haiku_kits_integration.h"
#include "RadeonAddonIntegration.h"
#include "RadeonReplicant.h"
#include <Deskbar.h>
#include <Roster.h>
#include <Entry.h>

void register_replicant()
{
    // ...
}

void haiku_send_set_mode(uint32_t width, uint32_t height)
{
    // Aquí implementaremos la llamada al driver/daemon
    printf("IPC: Set Mode requested: %dx%d\n", width, height);
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>

/* ------------------------------------------------------------------ */
/* Estado de conexiones IPC con radeon_gpud                           */
/* ------------------------------------------------------------------ */

static int g_gpud_fd = -1;
static int g_gpud_team = 1;

/* Abre conexión con radeon_gpud (vía shim o BPort) */
static int connect_to_gpud(void)
{
    if (g_gpud_fd >= 0)
        return g_gpud_fd;
    
    /* En Haiku real, usaríamos BPort. En desarrollo, shim in-proceso. */
    g_gpud_fd = open("/dev/graphics/radeon_gpud", O_RDWR);
    if (g_gpud_fd < 0) {
        /* Fallback: shim in-proceso para tests */
        g_gpud_fd = 0; /* marcador especial */
    }
    return g_gpud_fd;
}

static void disconnect_from_gpud(void)
{
    if (g_gpud_fd > 0) {
        close(g_gpud_fd);
        g_gpud_fd = -1;
    }
}

/* Envía comando IPC a radeon_gpud */
static status_t gpud_ipc(uint32_t msg, const void *payload, uint32_t payload_size,
                         void *reply, uint32_t reply_size)
{
    if (g_gpud_fd <= 0) {
        /* Modo simulación: usa shim in-proceso */
        extern int32_t rg_shim_team_for_fd(int);
        extern int32_t drmCommandWriteRead(int, unsigned long, void *, unsigned long);
        (void)rg_shim_team_for_fd;
        (void)drmCommandWriteRead;
        return B_OK; /* Simulado */
    }
    
    /* IPC real vía ioctl / BPort */
    return B_OK;
}

/* ------------------------------------------------------------------ */
/* 1. Media Kit Integration (UVD Hardware Video Decoding)             */
/* ------------------------------------------------------------------ */

bool haiku_media_kit_init_decoder(uint32_t codec_type, uint32_t width, uint32_t height)
{
    (void)codec_type; (void)width; (void)height;
    
    int fd = connect_to_gpud();
    if (fd < 0)
        return false;
    
    /* Solicita creación de contexto UVD al servidor */
    /* En producción: BO para bitstream, buffers de referencia, etc. */
    
    return true;
}

/* ------------------------------------------------------------------ */
/* 2. GL Kit / Mesa Integration (OpenGL 3.3 Context)                  */
/* ------------------------------------------------------------------ */

bool haiku_gl_kit_create_context(void *gl_view_handle)
{
    (void)gl_view_handle;
    
    int fd = connect_to_gpud();
    if (fd < 0)
        return false;
    
    /* Crea contexto GL vía radeon_gpud:
     * - Asigna VMID para el contexto
     * - Crea anillo de compute/graphics
     * - Configura shaders y pipeline
     */
    
    return true;
}

/* ------------------------------------------------------------------ */
/* 3. App Kit / app_server Integration (2D Hardware Acceleration)     */
/* ------------------------------------------------------------------ */

bool haiku_app_kit_blit_surface(const void *src_bits, void *dst_bits,
                                int width, int height)
{
    (void)src_bits; (void)dst_bits; (void)width; (void)height;
    
    if (!src_bits || !dst_bits || width <= 0 || height <= 0)
        return false;
    
    /* Blit 2D acelerado por hardware:
     * - Si src/dst están en VRAM: usa motor 2D (blit engine)
     * - Si están en memoria sistema: usa CPU fallback (SIMD)
     * 
     * Implementación real en radeon_supervisor / ASM kernels K6/K8
     */
    
    /* Fallback CPU simple para desarrollo */
    if (src_bits != dst_bits) {
        memcpy(dst_bits, src_bits, (size_t)width * (size_t)height * 4);
    }
    
    return true;
}

/* ------------------------------------------------------------------ */
/* 4. ROCm / ZLUDA / OpenCL Compute Runtime Integration               */
/* ------------------------------------------------------------------ */

bool haiku_rocm_init_compute_queue(uint32_t device_id)
{
    (void)device_id;
    
    int fd = connect_to_gpud();
    if (fd < 0)
        return false;
    
    /* Inicializa cola de compute para ROCm/ZLUDA:
     * - Crea MEC queue (GFX7+) o usa graphics pipeline (R600)
     * - Asigna doorbell, VMID, scratch buffer
     * - Configura amdgpu_shim para drmCommandWriteRead(AMDGPU_CS)
     */
    
    return true;
}

/* ------------------------------------------------------------------ */
/* Funciones adicionales para ciclo de vida                           */
/* ------------------------------------------------------------------ */

bool haiku_kits_shutdown(void)
{
    disconnect_from_gpud();
    return true;
}

const char *haiku_kits_version(void)
{
    return "0.1.0-dev";
}