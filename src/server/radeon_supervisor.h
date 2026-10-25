/*
 * radeon_supervisor.h - Supervisor en segundo plano para Watchdog, Defragmentación y Telemetría
 */
#ifndef RADEON_SUPERVISOR_H
#define RADEON_SUPERVISOR_H

#include <stdint.h>
#include <stdbool.h>
#include <thread>
#include <atomic>

class MemoryManager;
class RingManager;

class RadeonSupervisor {
public:
	RadeonSupervisor(MemoryManager &memory, RingManager &rings);
	~RadeonSupervisor();

	void start();
	void stop();

private:
	void supervisor_loop();

	MemoryManager &fMemory;
	RingManager &fRings;
	std::thread fThread;
	std::atomic<bool> fRunning{false};
};

#endif /* RADEON_SUPERVISOR_H */
