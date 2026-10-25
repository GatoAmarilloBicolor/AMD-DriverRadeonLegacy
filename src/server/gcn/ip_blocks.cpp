/*
 * ip_blocks.cpp - Implementacion del ciclo de vida de bloques IP.
 *
 * Los handlers son deliberadamente minimos: marcan el estado y dejan el
 * hardware para los backends (rg_legacy / rg_gcn). Lo que se valida aqui es
 * el ORDEN y la simetria init/fini y suspend/resume.
 */
#include "ip_blocks.h"

#include <cstdio>
#include <cstring>

static int ip_noop(rg_device *) { return RG_OK; }
static bool ip_idle(rg_device *) { return true; }

#define RG_IP_DEFAULT(nm_, ty_) \
	{ ty_, nm_, ip_noop, ip_noop, ip_noop, ip_noop, ip_noop, ip_noop, \
	  ip_noop, ip_noop, ip_idle, ip_noop }

static const rg_ip_block g_common = RG_IP_DEFAULT("common", RG_IP_COMMON);
static const rg_ip_block g_nbio = RG_IP_DEFAULT("nbio", RG_IP_NBIO);
static const rg_ip_block g_gmc = RG_IP_DEFAULT("gmc", RG_IP_GMC);
static const rg_ip_block g_ih = RG_IP_DEFAULT("ih", RG_IP_IH);
static const rg_ip_block g_psp = RG_IP_DEFAULT("psp", RG_IP_PSP);
static const rg_ip_block g_smu = RG_IP_DEFAULT("smu", RG_IP_SMU);
static const rg_ip_block g_gfx = RG_IP_DEFAULT("gfx", RG_IP_GFX);
static const rg_ip_block g_sdma = RG_IP_DEFAULT("sdma", RG_IP_SDMA);
static const rg_ip_block g_vcn = RG_IP_DEFAULT("vcn", RG_IP_VCN);

/* Legacy: sin PSP/SMU obligatorios en el arranque v1. */
static const rg_ip_block *kLegacyBlocks[] = {
	&g_common, &g_gmc, &g_ih, &g_gfx
};
/* GCN temprano. */
static const rg_ip_block *kGcnEarlyBlocks[] = {
	&g_common, &g_nbio, &g_gmc, &g_ih, &g_gfx, &g_sdma
};
/* Polaris/Vega. */
static const rg_ip_block *kGcnMidBlocks[] = {
	&g_common, &g_nbio, &g_gmc, &g_ih, &g_psp, &g_smu, &g_gfx, &g_sdma
};
/* RDNA. */
static const rg_ip_block *kGcnLateBlocks[] = {
	&g_common, &g_nbio, &g_gmc, &g_ih, &g_psp, &g_smu, &g_gfx, &g_sdma, &g_vcn
};

const rg_ip_block **rg_ip_blocks_for(rg_family family, uint32_t *count)
{
	const rg_ip_block **blocks;
	uint32_t n;
	if (family <= RG_FAMILY_NI) {
		blocks = kLegacyBlocks;
		n = sizeof(kLegacyBlocks) / sizeof(kLegacyBlocks[0]);
	} else if (family <= RG_FAMILY_GCN3) {
		blocks = kGcnMidBlocks;
		n = sizeof(kGcnMidBlocks) / sizeof(kGcnMidBlocks[0]);
	} else if (family <= RG_FAMILY_RDNA2) {
		blocks = kGcnLateBlocks;
		n = sizeof(kGcnLateBlocks) / sizeof(kGcnLateBlocks[0]);
	} else {
		blocks = kGcnLateBlocks;
		n = sizeof(kGcnLateBlocks) / sizeof(kGcnLateBlocks[0]);
	}
	(void)kGcnEarlyBlocks;
	if (count != nullptr)
		*count = n;
	return blocks;
}

int rg_device_init(rg_device *adev)
{
	if (adev == nullptr)
		return RG_EINVAL;
	for (uint32_t i = 0; i < adev->n_blocks; i++) {
		const rg_ip_block *b = adev->blocks[i];
		if (b->early_init != nullptr && b->early_init(adev) != RG_OK)
			return RG_EINVAL;
		if (b->sw_init != nullptr && b->sw_init(adev) != RG_OK)
			return RG_EINVAL;
		if (b->hw_init != nullptr && b->hw_init(adev) != RG_OK)
			return RG_EINVAL;
		if (b->late_init != nullptr && b->late_init(adev) != RG_OK)
			return RG_EINVAL;
	}
	adev->initialized = 1;
	return RG_OK;
}

int rg_device_fini(rg_device *adev)
{
	if (adev == nullptr)
		return RG_EINVAL;
	/* Orden inverso */
	for (uint32_t i = adev->n_blocks; i > 0; i--) {
		const rg_ip_block *b = adev->blocks[i - 1];
		if (b->hw_fini != nullptr)
			b->hw_fini(adev);
		if (b->sw_fini != nullptr)
			b->sw_fini(adev);
	}
	adev->initialized = 0;
	return RG_OK;
}

int rg_device_suspend(rg_device *adev)
{
	if (adev == nullptr) return RG_EINVAL;
	for (uint32_t i = adev->n_blocks; i > 0; i--)
		if (adev->blocks[i - 1]->suspend != nullptr)
			adev->blocks[i - 1]->suspend(adev);
	return RG_OK;
}

int rg_device_resume(rg_device *adev)
{
	if (adev == nullptr) return RG_EINVAL;
	for (uint32_t i = 0; i < adev->n_blocks; i++)
		if (adev->blocks[i]->resume != nullptr)
			adev->blocks[i]->resume(adev);
	return RG_OK;
}

int rg_device_soft_reset(rg_device *adev)
{
	if (adev == nullptr) return RG_EINVAL;
	for (uint32_t i = 0; i < adev->n_blocks; i++)
		if (adev->blocks[i]->soft_reset != nullptr)
			adev->blocks[i]->soft_reset(adev);
	return RG_OK;
}

bool rg_device_is_idle(rg_device *adev)
{
	if (adev == nullptr) return false;
	for (uint32_t i = 0; i < adev->n_blocks; i++) {
		const rg_ip_block *b = adev->blocks[i];
		if (b->is_idle != nullptr && !b->is_idle(adev))
			return false;
	}
	return true;
}

const char *rg_ip_type_name(rg_ip_type type)
{
	static const char *names[] = {
		"common", "nbio", "gmc", "ih", "psp", "smu", "gfx", "sdma", "dce", "vcn"
	};
	if (type >= 0 && type < RG_IP_TYPE_COUNT)
		return names[type];
	return "?";
}
