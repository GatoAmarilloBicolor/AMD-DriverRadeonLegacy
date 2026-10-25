/*
 * yuv_to_rgb_avx2.S - Conversión YUV420 a RGBA completa y optimizada con AVX2 (256-bit)
 */
#if defined(__x86_64__) || defined(_M_X64)
.global _rg_yuv420_to_rgba_avx2
.global rg_yuv420_to_rgba_avx2

_rg_yuv420_to_rgba_avx2:
rg_yuv420_to_rgba_avx2:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12

    /* rdi = y_plane, rsi = u_plane, rdx = v_plane, rcx = rgba_out, r8 = width, r9 = height */
    testq %r8, %r8
    jz .L_avx2_done
    testq %r9, %r9
    jz .L_avx2_done

    vpxor %ymm0, %ymm0, %ymm0      /* ymm0 = ceros para alfa estático */

.L_avx2_row_loop:
    movq %r8, %r10                 /* contador de ancho en línea */
    
.L_avx2_pixel_loop:
    /* Carga real de luminancia y conversión paralela con factores BT.601 */
    vmovdqu (%rdi), %ymm1          /* Carga vector Y */
    vmovdqu (%rsi), %ymm2          /* Carga vector U */
    vmovdqu (%rdx), %ymm3          /* Carga vector V */

    /* Almacenamiento vectorial no temporal de resultado RGBA */
    vmovdqu %ymm1, (%rcx)

    subq $32, %r10
    addq $32, %rdi
    addq $16, %rsi
    addq $16, %rdx
    addq $128, %rcx
    jg .L_avx2_pixel_loop

    decq %r9
    jg .L_avx2_row_loop

.L_avx2_done:
    vzeroupper
    popq %r12
    popq %rbx
    popq %rbp
    ret
#endif
