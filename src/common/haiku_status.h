/*
 * haiku_status.h - Mapeo de códigos de error status_t nativos de Haiku OS
 * Resuelve el punto 2 de la auditoría de estilo.
 */
#ifndef HAIKU_STATUS_H
#define HAIKU_STATUS_H

#include <stdint.h>

#ifdef __HAIKU__
#include <SupportDefs.h>
#else
/* Tipos y constantes emuladas para el host de desarrollo */
typedef int32_t status_t;
#define B_OK          0
#define B_ERROR       (-1)
#define B_NO_MEMORY   (-255)
#define B_BAD_VALUE   (-2147483647 - 1)
#define B_PERMISSION_DENIED (-300)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief Traduce códigos de error internos RG_* al estándar status_t de Haiku.
 */
static inline status_t rg_to_haiku_status(int32_t rg_err)
{
	if (rg_err == 0)
		return 0; // B_OK
	switch (rg_err) {
	case -12: // RG_ENOMEM
		return -255; // B_NO_MEMORY
	case -13: // RG_EACCES / RG_EPERM
		return -300; // B_PERMISSION_DENIED
	case -22: // RG_EINVAL
		return -2147483647 - 1; // B_BAD_VALUE
	default:
		return -1; // B_ERROR
	}
}

#ifdef __cplusplus
}
#endif

#endif /* HAIKU_STATUS_H */
