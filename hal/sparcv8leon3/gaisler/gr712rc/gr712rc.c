/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon3-gr712rc
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/tlb.h>

#include "hal/cpu.h"
#include "hal/interrupts.h"
#include "hal/tlb/tlb.h"
#include "hal/sparcv8leon3/sparcv8leon3.h"

#include "include/arch/gr712rc.h"

#include "config.h"

/* Clock gating unit */

#define VADDR_CGU (void *)((u32)VADDR_PERIPH_BASE + PAGE_OFFS_CGU)

#define CGU_UNLOCK     0 /* Unlock register        : 0x00 */
#define CGU_CLK_EN     1 /* Clock enable register  : 0x04 */
#define CGU_CORE_RESET 2 /* Core reset register    : 0x08 */


static struct {
	spinlock_t pltctlSp;
	volatile u32 *cguBase;
	intr_handler_t tlbIrqHandler;
} gr712rc_common;


volatile u32 hal_cpusStarted;


void hal_cpuHalt(void)
{
	/* GR712RC errata 1.7.8 */
	u32 addr = 0xfffffff0u;

	/* clang-format off */

	__asm__ volatile(
		"wr %%g0, %%asr19\n\t"
		"lda [%0] %c1, %%g0\n\t"
		:
		: "r"(addr), "i"(ASI_MMU_BYPASS)
	);
	/* clang-format on */
}


void hal_cpuInitCore(void)
{
	hal_tlbInitCore(hal_cpuGetID());
	hal_cpuAtomicInc(&hal_cpusStarted);
}


void _hal_cpuInit(void)
{
	hal_cpusStarted = 0;
	hal_cpuInitCore();
	hal_cpuStartCores();

	while (hal_cpusStarted != NUM_CPUS) {
	}
}


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn)
{
	(void)pin;
	(void)opt;
	(void)pullup;
	(void)pulldn;

	return 0;
}

/* CGU setup - section 28.2 GR712RC manual */

void _gr712rc_cguClkEnable(u32 device)
{
	u32 msk = 1 << device;

	*(gr712rc_common.cguBase + CGU_UNLOCK) |= msk;
	hal_cpuDataStoreBarrier();
	*(gr712rc_common.cguBase + CGU_CORE_RESET) |= msk;
	*(gr712rc_common.cguBase + CGU_CLK_EN) |= msk;
	*(gr712rc_common.cguBase + CGU_CORE_RESET) &= ~msk;
	hal_cpuDataStoreBarrier();
	*(gr712rc_common.cguBase + CGU_UNLOCK) &= ~msk;
}


void _gr712rc_cguClkDisable(u32 device)
{
	u32 msk = 1 << device;

	*(gr712rc_common.cguBase + CGU_UNLOCK) |= msk;
	hal_cpuDataStoreBarrier();
	*(gr712rc_common.cguBase + CGU_CORE_RESET) |= msk;
	*(gr712rc_common.cguBase + CGU_CLK_EN) &= ~msk;
	hal_cpuDataStoreBarrier();
	*(gr712rc_common.cguBase + CGU_UNLOCK) &= ~msk;
}


int _gr712rc_cguClkStatus(u32 device)
{
	u32 msk = 1 << device;

	return (*(gr712rc_common.cguBase + CGU_CLK_EN) & msk) ? 1 : 0;
}


void hal_wdgReload(void)
{
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = (platformctl_t *)ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&gr712rc_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_cguctrl:
			if (data->action == pctl_set) {
				if (data->cguctrl.state == disable) {
					_gr712rc_cguClkDisable(data->cguctrl.cgudev);
				}
				else {
					_gr712rc_cguClkEnable(data->cguctrl.cgudev);
				}
				ret = 0;
			}
			else if (data->action == pctl_get) {
				data->cguctrl.stateVal = _gr712rc_cguClkStatus(data->cguctrl.cgudev);
				ret = 0;
			}
			break;

		case pctl_iomux:
			if (data->action == pctl_set) {
				ret = gaisler_setIomuxCfg(data->iocfg.pin, data->iocfg.opt, data->iocfg.pullup, data->iocfg.pulldn);
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
	hal_spinlockClear(&gr712rc_common.pltctlSp, &sc);

	return ret;
}


void hal_cpuReboot(void)
{
	/* TODO */
	for (;;) {
	}

	__builtin_unreachable();
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&gr712rc_common.pltctlSp, "pltctl");

	gr712rc_common.cguBase = VADDR_CGU;

	gr712rc_common.tlbIrqHandler.f = hal_tlbIrqHandler;
	gr712rc_common.tlbIrqHandler.n = TLB_IRQ;
	gr712rc_common.tlbIrqHandler.data = NULL;

	hal_interruptsSetHandler(&gr712rc_common.tlbIrqHandler);
}
