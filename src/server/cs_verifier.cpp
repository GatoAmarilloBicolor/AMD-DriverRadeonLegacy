/*
 * cs_verifier.cpp - Verificador de command streams (C2 / §6.4).
 */
#include "cs_verifier.h"

#include <cstring>

/* Reglas por defecto: nada permitido. */
const rg_cs_policy rg_policy_deny_all = {
	"deny-all", nullptr, 0, nullptr, 0, 0
};

/*
 * Tabla de test. NOTA: los numeros son provisionales y sirven solo para
 * probar el motor. En produccion se generan desde r600d.h/r300_reg.h.
 */
static const rg_reg_rule kTestRegs[] = {
	{ 0x2000, 0x20FF, RG_REG_RELOC },  /* base de recursos (provisional) */
	{ 0x8000, 0x80FF, RG_REG_FREE },   /* estado de contexto (provisional) */
	{ 0x2800, 0x28FF, RG_REG_FORBID }  /* memory controller: prohibido */
};
static const rg_op_rule kTestOps[] = {
	{ 0x10, RG_OP_ALLOW }, /* NOP */
	{ 0x14, RG_OP_ALLOW }, /* SET_CONFIG_REG (provisional) */
	{ 0x15, RG_OP_ALLOW }, /* SET_CONTEXT_REG (provisional) */
	{ 0x2D, RG_OP_RELOC_ACCESS }, /* CP_DMA (real AMD PM4 opcode) */
	{ 0x33, RG_OP_RELOC_ACCESS }, /* DRAW (provisional) */
	{ 0x34, RG_OP_RELOC_ACCESS } /* DRAW_INDEX (provisional) */
};
const rg_cs_policy rg_policy_test = {
	"test", kTestRegs, sizeof(kTestRegs) / sizeof(kTestRegs[0]),
	kTestOps, sizeof(kTestOps) / sizeof(kTestOps[0]), 0x2000
};

CsVerifier::CsVerifier(const rg_cs_policy &policy)
	: fPolicy(&policy), fCache(std::make_unique<CsVerifyCache>())
{
}

int32_t CsVerifier::verify(const uint32_t *ib, uint32_t dwords,
	const rg_reloc *relocs, uint32_t n_relocs,
	rg_family family, const rg_bo_resolver &resolve,
	rg_verify_result *out) const
{
	return verify_impl(ib, dwords, relocs, n_relocs, family, resolve, out);
}