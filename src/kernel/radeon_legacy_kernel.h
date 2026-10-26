/*
 * radeon_legacy_kernel.h - Kernel-mode driver for Haiku.
 * Separated from userland: handles hardware init, memory management,
 * and provides a minimal interface to the user-space accelerant.
 */
#ifndef R_ADEON_LEGACY_KERNEL_H
#define R_ADEON_LEGACY_KERNEL_H

#include <Drivers.h>
#include <kernel/OS.h>
#include <media/kernel/Framebuffer.h>

#include "common/rg_types.h"
#include "server/memory_manager.h"

struct rg_asic_desc;

class RadeonLegacyKernel {
public:
    RadeonLegacyKernel();
    ~RadeonLegacyKernel();

    // Kernel driver entry points
    status_t init_driver(image_id driver_image);
    status_t uninit_driver();

    // Device hooks
    status_t device_open_hook(void** cookie);
    status_t device_close_hook(void* cookie);
    status_t device_free_hook(void* cookie);
    status_t device_control_hook(void* cookie, uint32 op, void* data, size_t length);

    // Device info
    status_t get_static_info(struct device_static_info* info);

    // Hardware initialization
    status_t detect_hardware();
    status_t setup_memory();

private:
    // PCI device info
    pci_module_info* fPCI;
    int32 fPCIIndex;

    // Hardware state
    rg_asic_desc fASIC;
    int32 fFD;  // File descriptor

    // Memory management
    MemoryManager fMemory;

    // Interrupt cookie
    interrupt_cookie fIRQCookie;

    // Framebuffer
    area_id fFBArea;
    void* fFBVirtual;
    uint32 fFBSize;

    // Connector state
    int32 fConnectorCount;
    bool fConnectors[4];
};

extern RadeonLegacyKernel* gKernelDriver;
