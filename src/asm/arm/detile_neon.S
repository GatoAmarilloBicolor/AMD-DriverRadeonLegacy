/*
 * detile_neon.S - Kernels NEON (A3 / §8A.5).
 * ABI AAPCS64: x0=dst, x1=src, x2=count(bloques de 16/64 bytes).
 */
#if defined(__APPLE__)
#  define SYM(x) _##x
#else
#  define SYM(x) x
#endif

.text
.p2align 2

/* void rg_asm_swizzle_neon(uint32_t *dst, const uint32_t *src, size_t blocks16) */
.globl SYM(rg_asm_swizzle_neon)
SYM(rg_asm_swizzle_neon):
	cbz x2, 2f
	adr x3, .Lswizzle_tbl
	ld1 {v2.16b}, [x3]
1:
	ld1 {v0.16b}, [x1], #16
	tbl v1.16b, {v0.16b}, v2.16b
	st1 {v1.16b}, [x0], #16
	subs x2, x2, #1
	b.ne 1b
2:
	ret
.p2align 4
.Lswizzle_tbl:
	.byte 3,0,1,2, 7,4,5,6, 11,8,9,10, 15,12,13,14

/* void rg_asm_copy_nt_neon(void *dst, const void *src, size_t blocks64) */
.globl SYM(rg_asm_copy_nt_neon)
SYM(rg_asm_copy_nt_neon):
	cbz x2, 2f
1:
	ld1 {v0.16b, v1.16b, v2.16b, v3.16b}, [x1], #64
	st1 {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], #64
	subs x2, x2, #1
	b.ne 1b
2:
	ret

/* void rg_asm_memset128_neon(void *dst, const uint8_t pattern[16], size_t blocks16) */
.globl SYM(rg_asm_memset128_neon)
SYM(rg_asm_memset128_neon):
	cbz x2, 2f
	ld1 {v0.16b}, [x1]
1:
	st1 {v0.16b}, [x0], #16
	subs x2, x2, #1
	b.ne 1b
2:
	ret

/* void rg_asm_swap_endian_neon(uint32_t *dst, const uint32_t *src, size_t blocks4) */
.globl SYM(rg_asm_swap_endian_neon)
SYM(rg_asm_swap_endian_neon):
	cbz x2, 2f
1:
	ld1 {v0.16b}, [x1], #16
	rev32 v0.16b, v0.16b
	st1 {v0.16b}, [x0], #16
	subs x2, x2, #1
	b.ne 1b
2:
	ret
