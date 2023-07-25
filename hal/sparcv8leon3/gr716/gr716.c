/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon3-gr716
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "gr716.h"
#include "../sparcv8leon3.h"
#include "../../cpu.h"
#include "../../spinlock.h"
#include "../../../include/arch/gr716.h"

#define GRGPREG_BASE ((void *)0x8000D000)
#define CGU_BASE0    ((void *)0x80006000)
#define CGU_BASE1    ((void *)0x80007000)


/* System configuration registers */

enum {
	cfg_gp0 = 0,   /* Sys IO config GPIO 0-7      : 0x00 */
	cfg_gp1,       /* Sys IO config GPIO 8-15     : 0x04 */
	cfg_gp2,       /* Sys IO config GPIO 16-23    : 0x08 */
	cfg_gp3,       /* Sys IO config GPIO 24-31    : 0x0C */
	cfg_gp4,       /* Sys IO config GPIO 32-39    : 0x10 */
	cfg_gp5,       /* Sys IO config GPIO 40-47    : 0x14 */
	cfg_gp6,       /* Sys IO config GPIO 48-55    : 0x18 */
	cfg_gp7,       /* Sys IO config GPIO 56-63    : 0x1C */
	cfg_pullup0,   /* Pull-up config GPIO 0-31    : 0x20 */
	cfg_pullup1,   /* Pull-up config GPIO 32-63   : 0x24 */
	cfg_pulldn0,   /* Pull-down config GPIO 0-31  : 0x28 */
	cfg_pulldn1,   /* Pull-down config GPIO 32-63 : 0x2C */
	cfg_lvds,      /* LVDS config                 : 0x30 */
	cfg_prot = 16, /* Sys IO config protection    : 0x40 */
	cfg_eirq,      /* Sys IO config err interrupt : 0x44 */
	cfg_estat      /* Sys IO config err status    : 0x48 */
};


/* Clock gating unit */

enum {
	cgu_unlock = 0, /* Unlock register        : 0x00 */
	cgu_clk_en,     /* Clock enable register  : 0x04 */
	cgu_core_reset, /* Core reset register    : 0x08 */
	cgu_override,   /* Override register - only primary CGU : 0x0C */
};


struct {
	spinlock_t pltctlSp;

	volatile u32 *grgpreg_base;
	volatile u32 *cgu_base0;
	volatile u32 *cgu_base1;
} gr716_common;


int _gr716_getIomuxCfg(u8 pin, u8 *opt, u8 *pullup, u8 *pulldn)
{
	if (pin > 63) {
		return -1;
	}

	*opt = (*(gr716_common.grgpreg_base + cfg_gp0 + (pin / 8)) >> ((pin % 8) << 2)) & 0xf;

	*pullup = (*(gr716_common.grgpreg_base + cfg_pullup0 + (pin / 32)) >> (pin % 32)) & 0x1;

	*pulldn = (*(gr716_common.grgpreg_base + cfg_pulldn0 + (pin / 32)) >> (pin % 32)) & 0x1;

	return 0;
}


int _gr716_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn)
{
	volatile u32 old_cfg;

	if (pin > 63) {
		return -1;
	}

	old_cfg = *(gr716_common.grgpreg_base + cfg_gp0 + (pin / 8));

	*(gr716_common.grgpreg_base + cfg_gp0 + (pin / 8)) =
		(old_cfg & ~(0xf << ((pin % 8) << 2))) | (opt << ((pin % 8) << 2));

	old_cfg = *(gr716_common.grgpreg_base + cfg_pullup0 + (pin / 32));

	*(gr716_common.grgpreg_base + cfg_pullup0 + (pin / 32)) =
		(old_cfg & ~(1 << (pin % 32))) | (pullup << (pin % 32));

	old_cfg = *(gr716_common.grgpreg_base + cfg_pulldn0 + (pin / 32));

	*(gr716_common.grgpreg_base + cfg_pulldn0 + (pin / 32)) =
		(old_cfg & ~(1 << (pin % 32))) | (pulldn << (pin % 32));

	return 0;
}


/* CGU setup - section 26.2 GR716 manual */

void _gr716_cguClkEnable(u32 cgu, u32 device)
{
	volatile u32 *cgu_base = (cgu == cgu_primary) ? gr716_common.cgu_base0 : gr716_common.cgu_base1;
	u32 msk = 1 << device;

	*(cgu_base + cgu_unlock) |= msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_core_reset) |= msk;
	*(cgu_base + cgu_clk_en) |= msk;
	*(cgu_base + cgu_clk_en) &= ~msk;
	*(cgu_base + cgu_core_reset) &= ~msk;
	*(cgu_base + cgu_clk_en) |= msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_unlock) &= ~msk;
}


void _gr716_cguClkDisable(u32 cgu, u32 device)
{
	volatile u32 *cgu_base = (cgu == cgu_primary) ? gr716_common.cgu_base0 : gr716_common.cgu_base1;
	u32 msk = 1 << device;

	*(cgu_base + cgu_unlock) |= msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_clk_en) &= ~msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_unlock) &= ~msk;
}


int _gr716_cguClkStatus(u32 cgu, u32 device)
{
	volatile u32 *cguBase = (cgu == cgu_primary) ? gr716_common.cgu_base0 : gr716_common.cgu_base1;
	u32 msk = 1 << device;

	return (*(cguBase + cgu_clk_en) & msk) ? 1 : 0;
}


void hal_wdgReload(void)
{
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = (platformctl_t *)ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&gr716_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_cguctrl:
			if (data->action == pctl_set) {
				if (data->cguctrl.state == disable) {
					_gr716_cguClkDisable(data->cguctrl.cgu, data->cguctrl.cgudev);
				}
				else {
					_gr716_cguClkEnable(data->cguctrl.cgu, data->cguctrl.cgudev);
				}
				ret = 0;
			}
			else if (data->action == pctl_get) {
				data->cguctrl.stateVal = _gr716_cguClkStatus(data->cguctrl.cgu, data->cguctrl.cgudev);
				ret = 0;
			}
			break;

		case pctl_iomux:
			if (data->action == pctl_set) {
				ret = _gr716_setIomuxCfg(data->iocfg.pin, data->iocfg.opt, data->iocfg.pullup, data->iocfg.pulldn);
			}
			else if (data->action == pctl_get) {
				ret = _gr716_getIomuxCfg(data->iocfg.pin, &data->iocfg.opt, &data->iocfg.pullup, &data->iocfg.pulldn);
			}
			break;

		case pctl_reboot:
			if ((data->action == pctl_set) && (data->reboot.magic == PCTL_REBOOT_MAGIC)) {
				hal_cpuReboot();
			}
			break;

		default:
			break;
	}
	hal_spinlockClear(&gr716_common.pltctlSp, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&gr716_common.pltctlSp, "pltctl");

	gr716_common.grgpreg_base = GRGPREG_BASE;
	gr716_common.cgu_base0 = CGU_BASE0;
	gr716_common.cgu_base1 = CGU_BASE1;
}
