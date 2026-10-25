/*
 * radeon_legacy_driver.cpp - Add-on gráfico nativo de Haiku OS para Radeon Legacy
 * Conecta el app_server con el demonio radeon_gpud y el helper de kernel.
 */
#include <GraphicsCard.h>
#include <Accelerant.h>
#include <SupportDefs.h>
#include <OS.h>

#ifdef __cplusplus
extern "C" {
#endif

status_t radeon_init_accelerant(int device_fd)
{
	/* Inicialización del acelerador gráfico nativo de Haiku */
	return B_OK;
}

void radeon_uninit_accelerant(void)
{
	/* Limpieza de recursos del acelerador */
}

#ifdef __cplusplus
}
#endif
