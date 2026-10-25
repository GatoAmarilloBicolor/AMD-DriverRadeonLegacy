/*
 * uvd_decoder.h - Soporte de Aceleración por Hardware UVD / UVD2 para Decodificación de Video en R600/R700
 */
#ifndef UVD_DECODER_H
#define UVD_DECODER_H

#include <stdint.h>
#include <stdbool.h>

enum uvd_codec {
	UVD_CODEC_H264 = 1,
	UVD_CODEC_VC1  = 2,
	UVD_CODEC_MPEG2= 3
};

struct uvd_decode_params {
	enum uvd_codec codec;
	uint32_t width;
	uint32_t height;
	uint32_t bitstream_size;
	const uint8_t *bitstream_data;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa y programa el bloque UVD de la GPU para decodificación por hardware */
bool uvd_hardware_decode_init(void);
bool uvd_submit_decode_job(const struct uvd_decode_params *params);

#ifdef __cplusplus
}
#endif

#endif /* UVD_DECODER_H */
