/*
 * ipc_zerocopy.cpp - Implementación del anillo circular de cero copia con memoria compartida
 */
#include "ipc_zerocopy.h"
#include <cstring>

bool rg_zerocopy_init(struct rg_zerocopy_ring *ring, uint32_t capacity)
{
	if (ring == nullptr || capacity == 0)
		return false;
	ring->magic = 0x5A5A0000u;
	ring->head.store(0, std::memory_order_relaxed);
	ring->tail.store(0, std::memory_order_relaxed);
	ring->capacity = capacity;
	ring->buffer_offset = sizeof(struct rg_zerocopy_ring);
	return true;
}

bool rg_zerocopy_push(struct rg_zerocopy_ring *ring, const void *data, uint32_t size)
{
	if (ring == nullptr || data == nullptr || size == 0)
		return false;
	
	uint32_t h = ring->head.load(std::memory_order_relaxed);
	uint32_t t = ring->tail.load(std::memory_order_acquire);
	
	if ((h + 1) % ring->capacity == t) {
		return false; /* Anillo lleno */
	}

	ring->head.store((h + 1) % ring->capacity, std::memory_order_release);
	return true;
}

bool rg_zerocopy_pop(struct rg_zerocopy_ring *ring, void *data, uint32_t *size)
{
	if (ring == nullptr || data == nullptr || size == nullptr)
		return false;

	uint32_t h = ring->head.load(std::memory_order_acquire);
	uint32_t t = ring->tail.load(std::memory_order_relaxed);

	if (h == t) {
		return false; /* Anillo vacío */
	}

	ring->tail.store((t + 1) % ring->capacity, std::memory_order_release);
	return true;
}
