/*
 * amdgpu_shim.c - Emulacion de amdgpu_drm.h sobre el mismo servidor.
 *
 * v1: capa de protocolo y contabilidad de timelines. El backend real de
 * anillos/GPUVM vive en src/server/gcn. Los ioctls aqui no hablan aun con
 * hardware; se usan para tests de contrato (G3).
 */
#include "amdgpu_shim.h"
#include "../common/rg_types.h"
#include "../server/asic/rg_asic.h"

#include <string.h>

static uint32_t g_cu_count = 36;      /* Polaris 10: 36 CUs */
static uint64_t g_va_max = 1ull << 40;

const char *amdgpu_shim_name(void) { return "amdgpu"; }

int amdgpu_shim_dev_info(struct amdgpu_dev_info *out)
{
	if (out == NULL)
		return RG_EINVAL;
	const rg_asic_desc *d = rg_asic_lookup(0x67DF, 0xC7, 0xFFFFFFFF);
	memset(out, 0, sizeof(*out));
	out->family = (uint32_t)d->family;
	out->gfx_level = d->gfx_level;
	out->cu_count = g_cu_count;
	out->max_se = 4;
	out->va_max = g_va_max;
	out->has_vm = (d->features & RG_ASIC_FEAT_VM) != 0;
	out->has_sdma = (d->features & RG_ASIC_FEAT_SDMA) != 0;
	out->has_mec = (d->features & RG_ASIC_FEAT_MEC) != 0;
	out->has_psp = (d->features & RG_ASIC_FEAT_PSP) != 0;
	out->has_smu = (d->features & RG_ASIC_FEAT_SMU) != 0;
	return RG_OK;
}

int amdgpu_shim_va_op(struct amdgpu_va_op *op)
{
	if (op == NULL)
		return RG_EINVAL;
	if (op->size == 0 || op->offset + op->size > g_va_max)
		return RG_EINVAL;
	if (op->offset & 0xFFF || op->size & 0xFFF)
		return RG_EINVAL;
	if (op->op > 1)
		return RG_EINVAL;
	return RG_OK;
}

/* Timelines: contador monotonico por handle; se valida el orden. */
#define RG_MAX_TIMELINES 128
static struct { uint32_t handle; uint64_t value; } g_timelines[RG_MAX_TIMELINES];

static int timeline_find(uint32_t handle)
{
	for (int i = 0; i < RG_MAX_TIMELINES; i++)
		if (g_timelines[i].handle == handle)
			return i;
	return -1;
}

static int timeline_alloc(uint32_t handle)
{
	for (int i = 0; i < RG_MAX_TIMELINES; i++) {
		if (g_timelines[i].handle == 0) {
			g_timelines[i].handle = handle;
			g_timelines[i].value = 0;
			return i;
		}
	}
	return -1;
}

int amdgpu_shim_timeline_signal(struct amdgpu_timeline *t)
{
	if (t == NULL || t->handle == 0)
		return RG_EINVAL;
	int i = timeline_find(t->handle);
	if (i < 0) {
		i = timeline_alloc(t->handle);
		if (i < 0)
			return RG_ENOMEM;
	}
	if (t->point <= g_timelines[i].value)
		return RG_EINVAL; /* los puntos deben crecer */
	g_timelines[i].value = t->point;
	t->value = t->point;
	return RG_OK;
}

int amdgpu_shim_timeline_wait(struct amdgpu_timeline *t, uint32_t timeout_ms)
{
	(void)timeout_ms;
	if (t == NULL)
		return RG_EINVAL;
	int i = timeline_find(t->handle);
	if (i < 0)
		return RG_ENOENT;
	if (g_timelines[i].value < t->point)
		return RG_EBUSY;
	t->value = g_timelines[i].value;
	return RG_OK;
}

int amdgpu_shim_timeline_transfer(struct amdgpu_timeline *t)
{
	if (t == NULL)
		return RG_EINVAL;
	int i = timeline_find(t->handle);
	if (i < 0)
		return RG_ENOENT;
	t->value = g_timelines[i].value;
	return RG_OK;
}
