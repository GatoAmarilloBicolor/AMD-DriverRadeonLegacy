/*
 * radeon_supervisor.cpp - Implementación del supervisor autónomo para producción
 */
#include "radeon_supervisor.h"
#include "memory_manager.h"
#include "ring_manager.h"
#include "radeon_hardening.h"
#include <chrono>

RadeonSupervisor::RadeonSupervisor(MemoryManager &memory, RingManager &rings)
	: fMemory(memory), fRings(rings)
{
}

RadeonSupervisor::~RadeonSupervisor()
{
	stop();
}

void RadeonSupervisor::start()
{
	if (fRunning)
		return;
	fRunning = true;
	fThread = std::thread(&RadeonSupervisor::supervisor_loop, this);
}

void RadeonSupervisor::stop()
{
	if (!fRunning)
		return;
	fRunning = false;
	if (fThread.joinable())
		fThread.join();
}

void RadeonSupervisor::supervisor_loop()
{
	while (fRunning) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		
		/* 1. Watchdog de Ring Buffers y monitoreo de colas */
		uint32_t pending_gfx = fRings.pending(RG_RING_GFX);
		if (pending_gfx > 100) {
			rg_kernel_debug_printf("[SUPERVISOR] Advertencia: Cola GFX saturada (%u IBs pendientes).", pending_gfx);
		}

		/* 2. Telemetría y estimación de saturación de bus PCIe (Punto 5 y 30) */
		uint64_t vram_used = fMemory.vram_used();
		uint64_t evictions = fMemory.evictions();
		rg_record_telemetry_metric("vram_used_bytes", vram_used);
		rg_record_telemetry_metric("eviction_count", evictions);
		
		if (evictions > 50) {
			rg_kernel_debug_printf("[SUPERVISOR] Alerta PCIe: Alto índice de desalojos VRAM->GTT (%llu). Posible saturación de bus.", (unsigned long long)evictions);
		}
	}
}
