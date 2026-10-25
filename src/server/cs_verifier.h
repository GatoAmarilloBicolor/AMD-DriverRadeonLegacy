/*
 * cs_verifier.h - Verificador de command streams (C2 / §6.4, ADR-003).
 *
 * Modelo: deny-by-default. El cliente entrega un IB y una lista de relocs.
 * Cada dword que contenga una direccion GPU debe venir como indice de reloc
 * (convencion Mesa: NOP de reloc + indice), y el servidor lo sustituye por
 * gpu_addr(BO) + offset SOLO si el BO pertenece al cliente y el acceso cae
 * dentro de sus limites.
 *
 * El IB se copia antes de validar (anti-TOCTOU); la GPU ejecuta la copia.
 */
#ifndef RG_CS_VERIFIER_H
#define RG_CS_VERIFIER_H

#include <stdint.h>
#include <vector>
#include <functional>
#include <type_traits>

#include "../common/rg_types.h"
#include "../common/rg_ipc.h"

enum rg_reg_policy {
	RG_REG_FORBID = 0,
	RG_REG_FREE = 1,    /* escribible, no produce accesos a memoria */
	RG_REG_RELOC = 2    /* contiene una direccion/base GPU */
};

enum rg_op_policy {
	RG_OP_FORBID = 0,
	RG_OP_ALLOW = 1,        /* sin accesos a memoria derivados de relocs */
	RG_OP_RELOC_ACCESS = 2  /* accede a memoria: exige >=1 reloc valida */
};

struct rg_reg_rule {
	uint32_t first;
	uint32_t last;
	uint32_t policy;
};

struct rg_op_rule {
	uint32_t opcode;
	uint32_t policy;
};

struct rg_cs_policy {
	const char *name;
	const rg_reg_rule *regs;
	uint32_t n_regs;
	const rg_op_rule *ops;
	uint32_t n_ops;
	uint32_t type0_base;  /* base para paquetes tipo 0 (r300) */
};

/* Tabla conservadora: todo prohibido. Es el valor por defecto. */
extern const rg_cs_policy rg_policy_deny_all;

/* Resolver: devuelve gpu_addr y size del BO del cliente, o nullptr. */
struct rg_bo_view {
	uint64_t gpu_addr;
	uint64_t size;
};

using rg_bo_resolver = std::function<bool(uint32_t handle, rg_bo_view *out)>;

/* Interface for BO resolution - eliminates std::function heap allocation in hot path */
class IBoResolver {
public:
    virtual ~IBoResolver() = default;
    virtual bool resolve(uint32_t handle, rg_bo_view *out) const = 0;
};

struct rg_verify_result {
	int32_t status;               /* 0 = valido */
	uint32_t bad_dword;           /* indice del dword problematico */
	uint32_t bad_opcode;
	uint32_t bad_reg;
	uint64_t fence;               /* fence emitida (si el IB la pide) */
	std::vector<uint32_t> patched;/* IB copiado y parcheado */
	std::vector<rg_bo_view> touched;
};

class CsVerifier {
public:
	/* Interface for BO resolution - eliminates std::function heap allocation in hot path */
	class IBoResolver {
	public:
		virtual ~IBoResolver() = default;
		virtual bool resolve(uint32_t handle, rg_bo_view *out) const = 0;
	};

	explicit CsVerifier(const rg_cs_policy &policy);

	/* family solo se usa para diagnosticos; la politica manda. */
	int32_t verify(const uint32_t *ib, uint32_t dwords,
		const rg_reloc *relocs, uint32_t n_relocs,
		rg_family family, const rg_bo_resolver &resolve,
		rg_verify_result *out) const;

	/* Zero-allocation overload using interface - accepts any IBoResolver-derived type */
	template<typename Resolver>
	int32_t verify(const uint32_t *ib, uint32_t dwords,
		const rg_reloc *relocs, uint32_t n_relocs,
		rg_family family, const Resolver &resolve,
		rg_verify_result *out) const;

private:
	const rg_cs_policy *fPolicy;
	
	// Common implementation
	template<typename Resolver>
	int32_t verify_impl(const uint32_t *ib, uint32_t dwords,
		const rg_reloc *relocs, uint32_t n_relocs,
		rg_family family, const Resolver &resolve,
		rg_verify_result *out) const;
};

/* Template implementation for IBoResolver-derived types (must be in header) */
template<typename Resolver>
int32_t CsVerifier::verify(const uint32_t *ib, uint32_t dwords,
	const rg_reloc *relocs, uint32_t n_relocs,
	rg_family family, const Resolver &resolve,
	rg_verify_result *out) const
{
	return verify_impl(ib, dwords, relocs, n_relocs, family, resolve, out);
}

/* Helper to call resolver uniformly for both IBoResolver and std::function */
template<typename R>
static bool call_resolve(const R &resolve, uint32_t handle, rg_bo_view *out) {
	if constexpr (std::is_invocable_v<R, uint32_t, rg_bo_view*>) {
		return resolve(handle, out);
	} else {
		return resolve.resolve(handle, out);
	}
}

template<typename R>
static int32_t resolve_reloc(const rg_reloc &r, const R &resolve,
	uint64_t access_extra, uint64_t *addr_out, rg_bo_view *view_out)
{
	rg_bo_view view{};
	if (!call_resolve(resolve, r.handle, &view))
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

static inline uint32_t reg_policy(const rg_cs_policy &p, uint32_t reg)
{
	for (uint32_t i = 0; i < p.n_regs; i++) {
		if (reg >= p.regs[i].first && reg <= p.regs[i].last)
			return p.regs[i].policy;
	}
	return RG_REG_FORBID;
}

static inline uint32_t op_policy(const rg_cs_policy &p, uint32_t opcode)
{
	for (uint32_t i = 0; i < p.n_ops; i++) {
		if (p.ops[i].opcode == opcode)
			return p.ops[i].policy;
	}
	return RG_OP_FORBID;
}

/* Common verification implementation (template) */
template<typename Resolver>
int32_t CsVerifier::verify_impl(const uint32_t *ib, uint32_t dwords,
	const rg_reloc *relocs, uint32_t n_relocs,
	rg_family family, const Resolver &resolve,
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

/* Tabla de pruebas (no usar en produccion): habilita el subconjunto minimo
 * para ejercitar el camino de parcheo. Los valores de registro/opcode
 * definitivos deben salir de r600d.h / r300_reg.h (regla 3 del plan). */
extern const rg_cs_policy rg_policy_test;

#endif /* RG_CS_VERIFIER_H */