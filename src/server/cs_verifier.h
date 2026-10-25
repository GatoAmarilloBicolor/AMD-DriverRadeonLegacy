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
	explicit CsVerifier(const rg_cs_policy &policy);

	/* family solo se usa para diagnosticos; la politica manda. */
	int32_t verify(const uint32_t *ib, uint32_t dwords,
		const rg_reloc *relocs, uint32_t n_relocs,
		rg_family family, const rg_bo_resolver &resolve,
		rg_verify_result *out) const;

private:
	const rg_cs_policy *fPolicy;
};

/* Tabla de pruebas (no usar en produccion): habilita el subconjunto minimo
 * para ejercitar el camino de parcheo. Los valores de registro/opcode
 * definitivos deben salir de r600d.h / r300_reg.h (regla 3 del plan). */
extern const rg_cs_policy rg_policy_test;

#endif /* RG_CS_VERIFIER_H */
