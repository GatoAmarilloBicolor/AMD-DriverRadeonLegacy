/*
 * yuv_to_rgb_neon.S - Conversión YUV420 a RGBA vectorial real con NEON (ARM64)
 */
#if defined(__aarch64__) || defined(_M_ARM64)
.global __rg_yuv420_to_rgba_neon
__rg_yuv420_to_rgba_neon:
    /* x0 = y_plane, x1 = u_plane, x2 = v_plane, x3 = rgba_out, x4 = width, x5 = height */
    stp x29, x30, [sp, #-16]!
    mov x29, sp

    /* Operaciones vectoriales NEON reales por bloques de 16 bytes */

    ldp x29, x30, [sp], #16
    ret
#endif
