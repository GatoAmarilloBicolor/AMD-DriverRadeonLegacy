/*
 * fast_blit_avx2.S - Operaciones de blit y copias masivas no temporales optimizadas con AVX2 (movntdq)
 */
#if defined(__x86_64__) || defined(_M_X64)
.global _rg_fast_copy_nt
.global rg_fast_copy_nt

_rg_fast_copy_nt:
rg_fast_copy_nt:
    pushq %rbp
    movq %rsp, %rbp
    
    /* Argumentos:
     * rdi = dst
     * rsi = src
     * rdx = size in bytes
     */

    testq %rdx, %rdx
    jz .L_copy_done

    /* Bucle optimizado con non-temporal hints para staging buffers VRAM */
.L_copy_loop:
    vmovdqu (%rsi), %ymm0
    vmovntdq %ymm0, (%rdi)
    addq $32, %rsi
    addq $32, %rdi
    subq $32, %rdx
    jg .L_copy_loop

    sfence

.L_copy_done:
    vzeroupper
    popq %rbp
    ret
#endif
