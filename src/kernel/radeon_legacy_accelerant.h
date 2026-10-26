/*
 * radeon_legacy_accelerant.h - Accelerant interface for Haiku.
 * Implements the accelerant that communicates with the kernel driver.
 */
#ifndef R_ADEON_LEGACY_ACCELERANT_H
#define R_ADEON_LEGACY_ACCELERANT_H

#include <Drivers.h>
#include "common/rg_types.h"
#include "server/memory_manager.h"

struct accelerant_info;
struct frame_buffer_config;

class RadeonLegacyAccelerant {
public:
    RadeonLegacyAccelerant(image_id driver_image);
    ~RadeonLegacyAccelerant();

    // Accelerant initialization
    status_t init_accelerant();
    status_t uninit_accelerant();

    // Accelerant hooks
    status_t get_info(struct accelerant_info* info);
    status_t get_mode_count(int32* count);
    status_t get_mode_list(int32 count, mode_* modes);
    status_t get_frame_buffer_config(frame_buffer_config* config);
    status_t set_mode(int32 mode);
    status_t set_dpms(int32 mode);
    status_t set_cursor(const unsigned char* data, int32 x, int32 y, int32 width, int32 height);
    status_t get_pixel_clock_limits(int32* min, int32* max);

    // Access to kernel driver
    RadeonLegacyKernel* kernel_driver() const { return fKernel; }

private:
    RadeonLegacyKernel* fKernel;
    image_id fDriverImage;
    status_t fAccelerantStatus;
};

#endif // R_ADEON_LEGACY_ACCELERANT_H
