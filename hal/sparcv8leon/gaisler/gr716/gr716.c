/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon-gr716
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/sparcv8leon/sparcv8leon.h"
#include "hal/cpu.h"
#include "hal/spinlock.h"

#include "hal/gaisler/ambapp.h"

#include "include/arch/sparcv8leon/gr716/gr716.h"

#include "gr716.h"

#define GRGPREG_BASE ((void *)0x8000d000)
#define CGU_BASE0    ((void *)0x80006000)
#define CGU_BASE1    ((void *)0x80007000)

#define BOOTSTRAP_ADDR 0x80008000
#define BOOTSTRAP_SPIM 0x400bc003


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


void hal_cpuHalt(void)
{
	/* must be performed in supervisor mode with int enabled */
	__asm__ volatile("wr %g0, %asr19");
}


void _hal_cpuInit(void)
{
}


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


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn)
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
				if (data->task.cguctrl.v.state == disable) {
					_gr716_cguClkDisable(data->task.cguctrl.cgu, data->task.cguctrl.cgudev);
				}
				else {
					_gr716_cguClkEnable(data->task.cguctrl.cgu, data->task.cguctrl.cgudev);
				}
				ret = 0;
			}
			else if (data->action == pctl_get) {
				data->task.cguctrl.v.stateVal = _gr716_cguClkStatus(data->task.cguctrl.cgu, data->task.cguctrl.cgudev);
				ret = 0;
			}
			break;

		case pctl_iomux:
			if (data->action == pctl_set) {
				ret = gaisler_setIomuxCfg(data->task.iocfg.pin, data->task.iocfg.opt, data->task.iocfg.pullup, data->task.iocfg.pulldn);
			}
			else if (data->action == pctl_get) {
				ret = _gr716_getIomuxCfg(data->task.iocfg.pin, &data->task.iocfg.opt, &data->task.iocfg.pullup, &data->task.iocfg.pulldn);
			}
			break;

		case pctl_ambapp:
			if (data->action == pctl_get) {
				ret = ambapp_findSlave(data->task.ambapp.dev, data->task.ambapp.instance);
			}
			break;

		case pctl_reboot:
			if ((data->action == pctl_set) && (data->task.reboot.magic == PCTL_REBOOT_MAGIC)) {
				hal_cpuReboot();
			}
			break;

		default:
			break;
	}
	hal_spinlockClear(&gr716_common.pltctlSp, &sc);

	return ret;
}


void hal_cpuReboot(void)
{
	/* Reset to the built-in bootloader */
	hal_cpuDisableInterrupts();

	/* Reboot to SPIM */
	*(volatile u32 *)(BOOTSTRAP_ADDR) = BOOTSTRAP_SPIM;

	/* clang-format off */
	__asm__ volatile (
		"jmp %%g0\n\t"
		"nop\n\t"
		:::
	);
	/* clang-format on */

	__builtin_unreachable();
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&gr716_common.pltctlSp, "pltctl");

	gr716_common.grgpreg_base = GRGPREG_BASE;
	gr716_common.cgu_base0 = CGU_BASE0;
	gr716_common.cgu_base1 = CGU_BASE1;

	ambapp_init();
}
