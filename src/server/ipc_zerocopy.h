/*
 * ipc_zerocopy.h - Protocolo IPC de Cero Copia mediante Anillo Circular Compartido y Atómicos
 */
#ifndef IPC_ZEROCOPY_H
#define IPC_ZEROCOPY_H

#include <stdint.h>
#include <stdbool.h>
#include <atomic>

struct rg_zerocopy_ring {
	uint32_t magic;
	std::atomic<uint32_t> head;
	std::atomic<uint32_t> tail;
	uint32_t capacity;
	uint32_t buffer_offset;
};

#ifdef __cplusplus
extern "C" {
#endif

bool rg_zerocopy_init(struct rg_zerocopy_ring *ring, uint32_t capacity);
bool rg_zerocopy_push(struct rg_zerocopy_ring *ring, const void *data, uint32_t size);
bool rg_zerocopy_pop(struct rg_zerocopy_ring *ring, void *data, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif /* IPC_ZEROCOPY_H */
