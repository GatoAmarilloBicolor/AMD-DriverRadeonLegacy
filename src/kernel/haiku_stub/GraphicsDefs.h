/*
 * GraphicsDefs.h - Host-only stand-in for Haiku's <GraphicsDefs.h>.
 * Only the declarations used by the accelerant are provided.
 */
#ifndef _GRAPHICS_DEFS_H
#define _GRAPHICS_DEFS_H

#include <SupportDefs.h>

typedef struct rgb_color {
	uint8	red;
	uint8	green;
	uint8	blue;
	uint8	alpha;
} rgb_color;

typedef enum {
	B_NO_COLOR_SPACE	= 0x0000,
	B_RGBA64			= 0x2012,
	B_RGB48				= 0x0011,
	B_RGB32				= 0x0008,
	B_RGBA32			= 0x2008,
	B_RGB30				= 0x0009,
	B_RGB24				= 0x0003,
	B_RGB16				= 0x0005,
	B_RGB15				= 0x0010,
	B_RGBA15			= 0x2010,
	B_CMAP8				= 0x0004,
	B_GRAY8				= 0x0002,
	B_GRAY1				= 0x0001,
	B_RGB32_BIG			= 0x1008,
	B_RGBA32_BIG		= 0x3008,
	B_RGB24_BIG			= 0x1003,
	B_RGB16_BIG			= 0x1005,
	B_RGB15_BIG			= 0x1010,
	B_RGBA15_BIG		= 0x3010,
	B_MONOCHROME_1_BIT	= B_GRAY1,
	B_GRAYSCALE_8_BIT	= B_GRAY8,
	B_COLOR_8_BIT		= B_CMAP8,
	B_RGB_32_BIT		= B_RGB32,
	B_RGB_16_BIT		= B_RGB15,
	B_BIG_RGB_32_BIT	= B_RGB32_BIG,
	B_BIG_RGB_16_BIT	= B_RGB15_BIG
} color_space;

#endif	/* _GRAPHICS_DEFS_H */
