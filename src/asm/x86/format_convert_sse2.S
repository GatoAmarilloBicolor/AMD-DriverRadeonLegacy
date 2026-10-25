/*
 * format_convert_sse2.S - Kernels AVX2 (A2 / §8A.5).
 * ABI System V: rdi=dst, rsi=src, rdx=count(bloques de 32).
 */
#if defined(__APPLE__)
#  define SYM(x) _##x
#else
#  define SYM(x) x
#endif

.text
.p2align 4

/* void rg_asm_swizzle_avx2(uint32_t *dst, const uint32_t *src, size_t blocks32) */
.globl SYM(rg_asm_swizzle_avx2)
SYM(rg_asm_swizzle_avx2):
	test %rdx, %rdx
	jz 2f
	vmovdqa .Lswizzle_mask(%rip), %ymm1
1:
	vmovdqu (%rsi), %ymm0
	vpshufb %ymm1, %ymm0, %ymm0
	vmovdqu %ymm0, (%rdi)
	add $32, %rsi
	add $32, %rdi
	dec %rdx
	jnz 1b
	vzeroupper
2:
	ret

/* void rg_asm_copy_nt_avx2(void *dst, const void *src, size_t blocks32) */
.globl SYM(rg_asm_copy_nt_avx2)
SYM(rg_asm_copy_nt_avx2):
	test %rdx, %rdx
	jz 2f
1:
	vmovdqu (%rsi), %ymm0
	vmovntdq %ymm0, (%rdi)
	add $32, %rsi
	add $32, %rdi
	dec %rdx
	jnz 1b
	vzeroupper
	sfence
2:
	ret

#if defined(__APPLE__)
.section __TEXT,__const
#else
.section .rodata
#endif
.p2align 5
.Lswizzle_mask:
	.byte 3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14
	.byte 3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14

/* void rg_asm_memset128_avx2(void *dst, const uint8_t pattern[16], size_t blocks32) */
.globl SYM(rg_asm_memset128_avx2)
SYM(rg_asm_memset128_avx2):
	test %rdx, %rdx
	jz 2f
	vmovdqu (%rsi), %xmm0
	vinserti128 $1, %xmm0, %ymm0, %ymm0
1:
	vmovdqu %ymm0, (%rdi)
	add $32, %rdi
	dec %rdx
	jnz 1b
	vzeroupper
2:
	ret

/* void rg_asm_swap_endian_avx2(uint32_t *dst, const uint32_t *src, size_t blocks8) */
.globl SYM(rg_asm_swap_endian_avx2)
SYM(rg_asm_swap_endian_avx2):
	test %rdx, %rdx
	jz 2f
	vmovdqa .Lswap_mask(%rip), %ymm1
1:
	vmovdqu (%rsi), %ymm0
	vpshufb %ymm1, %ymm0, %ymm0
	vmovdqu %ymm0, (%rdi)
	add $32, %rsi
	add $32, %rdi
	dec %rdx
	jnz 1b
	vzeroupper
2:
	ret

.p2align 5
.Lswap_mask:
	.byte 3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12
	.byte 3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12
