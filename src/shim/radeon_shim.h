/*
 * radeon_shim.h - libdrm_haiku: contrato publico del shim (C3 / §6.2).
 *
 * El shim traduce los ioctls de radeon_drm.h a mensajes RG_* del servidor.
 * En Haiku el transporte es un BPort; en el host se inyecta una funcion.
 */
#ifndef RG_RADEON_SHIM_H
#define RG_RADEON_SHIM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t area_id;

/* Transporte: envia un mensaje y recibe la respuesta.
 * Devuelve bytes de respuesta o error negativo. */
typedef int32_t (*rg_transport_fn)(uint32_t team, uint32_t msg,
	const void *payload, uint32_t payload_size, void *reply, uint32_t reply_size);

void rg_shim_set_transport(rg_transport_fn fn);

/* Identifica al cliente (team_id). Sin llamar, se usa 1. */
void rg_shim_set_team(uint32_t team);

/* fd falso por cliente. */
int rg_shim_open(void);
int rg_shim_close(int fd);
int rg_shim_team_for_fd(int fd);

/* Entradas que Mesa usa a traves de drmCommandWriteRead / drmIoctl. */
int drmCommandWriteRead(int fd, unsigned long request, void *data, unsigned long size);
int drmIoctl(int fd, unsigned long request, void *arg);
void *drmMmap(int fd, void *addr, size_t length, int prot, int flags, int64_t offset);

#ifdef __cplusplus
}
#endif

#endif /* RG_RADEON_SHIM_H */
