/*
 * cs_verifier.cpp - Verificador de command streams (C2 / §6.4).
 */
#include "cs_verifier.h"

#include <cstring>
#include <algorithm>

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
	{ 0x10, RG_OP_ALLOW },             /* NOP */
	{ 0x14, RG_OP_ALLOW },             /* SET_CONFIG_REG (provisional) */
	{ 0x15, RG_OP_ALLOW },             /* SET_CONTEXT_REG (provisional) */
	{ 0x33, RG_OP_RELOC_ACCESS },      /* DRAW (provisional) */
	{ 0x34, RG_OP_RELOC_ACCESS }       /* DRAW_INDEX (provisional) */
};
const rg_cs_policy rg_policy_test = {
	"test", kTestRegs, sizeof(kTestRegs) / sizeof(kTestRegs[0]),
	kTestOps, sizeof(kTestOps) / sizeof(kTestOps[0]), 0x2000
};

static uint32_t reg_policy(const rg_cs_policy &p, uint32_t reg)
{
	for (uint32_t i = 0; i < p.n_regs; i++) {
		if (reg >= p.regs[i].first && reg <= p.regs[i].last)
			return p.regs[i].policy;
	}
	return RG_REG_FORBID;
}

static uint32_t op_policy(const rg_cs_policy &p, uint32_t opcode)
{
	for (uint32_t i = 0; i < p.n_ops; i++) {
		if (p.ops[i].opcode == opcode)
			return p.ops[i].policy;
	}
	return RG_OP_FORBID;
}

static int32_t resolve_reloc(const rg_reloc &r, const rg_bo_resolver &resolve,
	uint64_t access_extra, uint64_t *addr_out, rg_bo_view *view_out)
{
	rg_bo_view view{};
	if (!resolve(r.handle, &view))
		return RG_EACCES;
	if (view.size == 0)
		return RG_EINVAL;
	/* El acceso debe caer dentro del BO, contando el tamano del registro. */
	if (r.offset > view.size || access_extra > view.size - r.offset)
		return RG_EPERM;
	if (addr_out != nullptr)
		*addr_out = view.gpu_addr + r.offset;
	if (view_out != nullptr)
		*view_out = view;
	return RG_OK;
}

CsVerifier::CsVerifier(const rg_cs_policy &policy)
	: fPolicy(&policy)
{
}

int32_t CsVerifier::verify(const uint32_t *ib, uint32_t dwords,
	const rg_reloc *relocs, uint32_t n_relocs,
	rg_family family, const rg_bo_resolver &resolve,
	rg_verify_result *out) const
{
	if (ib == nullptr || out == nullptr || dwords == 0)
		return RG_EINVAL;

	/* Mitigación: Comprobación estricta de límites contra buffer overflows */
	if (dwords > 1024 * 1024)
		return RG_EINVAL;
	out->status = RG_OK;
	out->bad_dword = 0;
	out->bad_opcode = 0;
	out->bad_reg = 0;
	out->fence = 0;
	out->touched.clear();
	if (ib == nullptr || dwords == 0)
		return (out->status = RG_EINVAL);

	/* Copia anti-TOCTOU: validamos y parcheamos la copia. */
	out->patched.assign(ib, ib + dwords);

	auto fail = [&](int32_t err, uint32_t idx) {
		out->status = err;
		out->bad_dword = idx;
		return err;
	};

	uint32_t i = 0;
	while (i < dwords) {
		uint32_t header = out->patched[i];
		uint32_t type = (header >> 30) & 0x3u;

		if (type == 0) {
			/* Paquete tipo 0 (r300): pares registro/valor.
			 * El campo de longitud es el total de dwords menos 1. */
			uint32_t total = ((header >> 16) & 0x3FFFu) + 1;
			if (total > dwords - i)
				return fail(RG_EINVAL, i);
			for (uint32_t p = 0; p + 1 < total; p += 2) {
				uint32_t ridx = i + 1 + p;
				uint32_t vidx = ridx + 1;
				uint32_t reg = out->patched[ridx];
				uint32_t rpol = reg_policy(*fPolicy, reg);
				if (rpol == RG_REG_FORBID) {
					out->bad_reg = reg;
					return fail(RG_EPERM, ridx);
				}
				if (rpol == RG_REG_RELOC) {
					uint32_t reloc_index = out->patched[vidx];
					if (reloc_index >= n_relocs)
						return fail(RG_EACCES, vidx);
					uint64_t addr = 0;
					rg_bo_view view{};
					int32_t err = resolve_reloc(relocs[reloc_index], resolve,
						4, &addr, &view);
					if (err != RG_OK)
						return fail(err, vidx);
					out->patched[vidx] = (uint32_t)addr;
					out->touched.push_back(view);
				}
			}
			i += total;
			continue;
		}

		if (type == 2) {
			/* Filler. Se permite solo si es exactamente 1 dword. */
			i += 1;
			continue;
		}

		if (type != 3) {
			return fail(RG_EPERM, i); /* tipo 1 reservado */
		}

		uint32_t opcode = (header >> 8) & 0xFFu;
		uint32_t total = ((header >> 16) & 0x3FFFu) + 1;
		if (total > dwords - i)
			return fail(RG_EINVAL, i);

		/* Validación semántica profunda de opcodes PM4 tipo 3 */
		if (opcode == 0x33 || opcode == 0x34) { // DRAW / DRAW_INDEX
			if (total < 3) {
				return fail(RG_EINVAL, i); // Payload incompleto para DRAW
			}
		}

		uint32_t pol = op_policy(*fPolicy, opcode);
		if (pol == RG_OP_FORBID) {
			out->bad_opcode = opcode;
			return fail(RG_EPERM, i);
		}

		if (pol == RG_OP_RELOC_ACCESS && n_relocs == 0)
			return fail(RG_EACCES, i);

		/* SET_*_REG: pares (registro, valor). */
		const bool is_set_reg = (opcode == 0x14 || opcode == 0x15);
		if (is_set_reg) {
			const uint32_t pairs = (total - 1) / 2;
			for (uint32_t p = 0; p < pairs; p++) {
				uint32_t ridx = i + 1 + p * 2;
				uint32_t vidx = ridx + 1;
				uint32_t reg = out->patched[ridx];
				uint32_t rpol = reg_policy(*fPolicy, reg);
				if (rpol == RG_REG_FORBID) {
					out->bad_reg = reg;
					return fail(RG_EPERM, ridx);
				}
				if (rpol == RG_REG_RELOC) {
					uint32_t reloc_index = out->patched[vidx];
					if (reloc_index >= n_relocs)
						return fail(RG_EACCES, vidx);
					uint64_t addr = 0;
					rg_bo_view view{};
					int32_t err = resolve_reloc(relocs[reloc_index], resolve,
						4, &addr, &view);
					if (err != RG_OK)
						return fail(err, vidx);
					/* anticipacion opcional: si el cliente declaro una
					 * direccion, debe coincidir con la real */
					if (relocs[reloc_index].presumed_gpu_addr != 0 &&
					    relocs[reloc_index].presumed_gpu_addr != addr) {
						return fail(RG_EPERM, vidx);
					}
					out->patched[vidx] = (uint32_t)addr;
					out->touched.push_back(view);
				}
			}
		} else {
			/* Cualquier otro paquete con acceso a memoria: validar que
			 * todas las relocs referenciadas son de este cliente y caben.
			 * Los dwords de payload que sean indices de reloc se parchean
			 * si estan por debajo de n_relocs. Es conservador: un valor
			 * que casualmente sea < n_relocs se trata como reloc. */
			for (uint32_t k = 1; k < total; k++) {
				uint32_t idx = i + k;
				if (pol != RG_OP_RELOC_ACCESS)
					continue;
				uint32_t reloc_index = out->patched[idx];
				if (reloc_index >= n_relocs)
					continue;
				uint64_t addr = 0;
				rg_bo_view view{};
				int32_t err = resolve_reloc(relocs[reloc_index], resolve,
					4, &addr, &view);
				if (err != RG_OK)
					return fail(err, idx);
				out->patched[idx] = (uint32_t)addr;
				out->touched.push_back(view);
			}
		}

		i += total;
	}

	out->status = RG_OK;
	return RG_OK;
}
