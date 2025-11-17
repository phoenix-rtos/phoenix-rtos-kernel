/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon-generic
 *
 * Copyright 2024 Phoenix Systems
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
#include "hal/sparcv8leon/sparcv8leon.h"

#include "hal/gaisler/ambapp.h"

#include "include/arch/sparcv8leon/generic/generic.h"

#include "config.h"


static struct {
	spinlock_t pltctlSp;
	intr_handler_t tlbIrqHandler;
	volatile u32 hal_cpusStarted;
} generic_common;


void hal_cpuHalt(void)
{
	/* clang-format off */
	__asm__ volatile ("wr %g0, %asr19");
	/* clang-format on */
}


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Function is called from assembly" */
void hal_cpuInitCore(void)
{
	hal_tlbInitCore(hal_cpuGetID());
	hal_cpuAtomicInc(&generic_common.hal_cpusStarted);
}


void _hal_cpuInit(void)
{
	generic_common.hal_cpusStarted = 0;
	hal_cpuInitCore();
	hal_cpuStartCores();

	while (generic_common.hal_cpusStarted != NUM_CPUS) {
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


void hal_wdgReload(void)
{
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = (platformctl_t *)ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&generic_common.pltctlSp, &sc);

	switch (data->type) {
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
			/* No action required*/
			break;
	}
	hal_spinlockClear(&generic_common.pltctlSp, &sc);

	return ret;
}


__attribute__((noreturn)) void hal_cpuReboot(void)
{
	/* TODO */
	for (;;) {
	}

	__builtin_unreachable();
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&generic_common.pltctlSp, "pltctl");

	generic_common.tlbIrqHandler.f = hal_tlbIrqHandler;
	generic_common.tlbIrqHandler.n = TLB_IRQ;
	generic_common.tlbIrqHandler.data = NULL;

	(void)hal_interruptsSetHandler(&generic_common.tlbIrqHandler);

	ambapp_init();
}
