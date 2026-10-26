/*
 * radeon_legacy_fb_ops.h - Framebuffer operations for Haiku.
 * Provides the interface between the kernel driver and the framebuffer.
 */
#ifndef R_ADEON_LEGACY_FB_OPS_H
#define R_ADEON_LEGACY_FB_OPS_H

#include <SupportDefs.h>
#include <Drivers.h>

struct frame_buffer_config {
    int32 virtual_count;
    int32 virtual_pos;
    // Additional fields would be added for a complete implementation
};

status_t map_frame_buffer(void* cookie, off_t offset, void** _data, size_t B_SIZE, uint32 flags);
status_t unmap_frame_buffer(void* cookie, void* data, size_t B_SIZE);
status_t get_frame_buffer_config(void* cookie, struct frame_buffer_config* config);
status_t set_frame_buffer_config(void* cookie, const struct frame_buffer_config* config);

#endif // R_ADEON_LEGACY_FB_OPS_H
