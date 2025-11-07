/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon-gr740
 *
 * Copyright 2025 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/tlb.h>

#include "hal/cpu.h"
#include "hal/hal.h"
#include "hal/interrupts.h"
#include "hal/tlb/tlb.h"
#include "hal/sparcv8leon/gaisler/l2cache.h"

#include "include/arch/sparcv8leon/gr740/gr740.h"

#include "config.h"

#define L2C_BASE ((addr_t)0xf0000000)

/* Clock gating unit */

#define CGU_BASE ((void *)0xffa04000)

enum {
	cgu_unlock = 0, /* Unlock register        : 0x00 */
	cgu_clk_en,     /* Clock enable register  : 0x04 */
	cgu_core_reset, /* Core reset register    : 0x08 */
	cgu_override,   /* Override register      : 0x0c */
};

/* I/O & PLL configuration registers */

#define GRGPREG_BASE ((void *)0xffa0b000)

enum {
	ftmfunc = 0, /* FTMCTRL function enable register       : 0x00 */
	altfunc,     /* Alternate function enable register     : 0x04 */
	lvdsmclk,    /* LVDS and mem CLK pad enable register   : 0x08 */
	pllnewcfg,   /* PLL new configuration register         : 0x0c */
	pllrecfg,    /* PLL reconfigure command register       : 0x10 */
	pllcurcfg,   /* PLL current configuration register     : 0x14 */
	drvstr1,     /* Drive strength register 1              : 0x18 */
	drvstr2,     /* Drive strength register 2              : 0x1c */
	lockdown,    /* Configuration lockdown register        : 0x20 */
};


static struct {
	spinlock_t pltctlSp;
	volatile u32 *cguBase;
	volatile u32 *grgpregBase;
	intr_handler_t tlbIrqHandler;
} gr740_common;


volatile u32 hal_cpusStarted;


void hal_cpuHalt(void)
{
	/* clang-format off */
	__asm__ volatile ("wr %g0, %asr19");
	/* clang-format on */
}


void hal_cpuInitCore(void)
{
	hal_tlbInitCore(hal_cpuGetID());
	/* Enable cycle counter */
	/* clang-format off */
	__asm__ volatile ("wr %g0, %asr22");
	/* clang-format on */
	hal_cpuAtomicInc(&hal_cpusStarted);
}


void _hal_cpuInit(void)
{
	hal_cpusStarted = 0;
	hal_cpuInitCore();
	hal_cpuStartCores();

	while (hal_cpusStarted != NUM_CPUS) {
	}

	l2c_init(L2C_BASE);
	l2c_flushRange(l2c_inv_all, 0, 0);
}


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn)
{
	if (pin > 21) {
		return -1;
	}

	switch (opt) {
		case iomux_gpio:
			*(gr740_common.grgpregBase + ftmfunc) &= ~(1 << pin);
			*(gr740_common.grgpregBase + altfunc) &= ~(1 << pin);
			break;

		case iomux_alternateio:
			*(gr740_common.grgpregBase + ftmfunc) &= ~(1 << pin);
			*(gr740_common.grgpregBase + altfunc) |= 1 << pin;
			break;

		case iomux_promio:
			*(gr740_common.grgpregBase + ftmfunc) |= 1 << pin;
			break;

		default:
			return -1;
	}

	return 0;
}

/* CGU setup - section 25.2 GR740 manual */

void _gr740_cguClkEnable(u32 device)
{
	u32 msk = 1 << device;

	*(gr740_common.cguBase + cgu_unlock) |= msk;
	*(gr740_common.cguBase + cgu_core_reset) |= msk;
	*(gr740_common.cguBase + cgu_clk_en) |= msk;
	*(gr740_common.cguBase + cgu_clk_en) &= ~msk;
	*(gr740_common.cguBase + cgu_core_reset) &= ~msk;
	*(gr740_common.cguBase + cgu_clk_en) |= msk;
	*(gr740_common.cguBase + cgu_unlock) &= ~msk;
}


void _gr740_cguClkDisable(u32 device)
{
	u32 msk = 1 << device;

	*(gr740_common.cguBase + cgu_unlock) |= msk;
	*(gr740_common.cguBase + cgu_clk_en) &= ~msk;
	*(gr740_common.cguBase + cgu_unlock) &= ~msk;
}


int _gr740_cguClkStatus(u32 device)
{
	u32 msk = 1 << device;

	return (*(gr740_common.cguBase + cgu_clk_en) & msk) ? 1 : 0;
}


void hal_wdgReload(void)
{
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = (platformctl_t *)ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&gr740_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_cguctrl:
			if (data->action == pctl_set) {
				if (data->task.cguctrl.v.state == disable) {
					_gr740_cguClkDisable(data->task.cguctrl.cgudev);
				}
				else {
					_gr740_cguClkEnable(data->task.cguctrl.cgudev);
				}
				ret = 0;
			}
			else if (data->action == pctl_get) {
				data->task.cguctrl.v.stateVal = _gr740_cguClkStatus(data->task.cguctrl.cgudev);
				ret = 0;
			}
			break;

		case pctl_iomux:
			if (data->action == pctl_set) {
				ret = gaisler_setIomuxCfg(data->task.iocfg.pin, data->task.iocfg.opt, data->task.iocfg.pullup, data->task.iocfg.pulldn);
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
	hal_spinlockClear(&gr740_common.pltctlSp, &sc);

	return ret;
}


void hal_cpuReboot(void)
{
	hal_timerWdogReboot();
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&gr740_common.pltctlSp, "pltctl");

	gr740_common.cguBase = _pmap_halMapDevice(PAGE_ALIGN(CGU_BASE), PAGE_OFFS(CGU_BASE), SIZE_PAGE);
	gr740_common.grgpregBase = _pmap_halMapDevice(PAGE_ALIGN(GRGPREG_BASE), PAGE_OFFS(GRGPREG_BASE), SIZE_PAGE);

	gr740_common.tlbIrqHandler.f = hal_tlbIrqHandler;
	gr740_common.tlbIrqHandler.n = TLB_IRQ;
	gr740_common.tlbIrqHandler.data = NULL;

	hal_interruptsSetHandler(&gr740_common.tlbIrqHandler);

	ambapp_init();
}
