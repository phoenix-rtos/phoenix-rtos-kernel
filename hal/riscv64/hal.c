/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "hal/spinlock.h"
#include "hal/console.h"
#include "hal/exceptions.h"
#include "hal/interrupts.h"
#include "hal/cpu.h"
#include "hal/pmap.h"
#include "hal/timer.h"
#include "config.h"
#include "halsyspage.h"

#include "include/arch/riscv64.h"

static struct {
	int started;
	spinlock_t pltctlSp;
} hal_common;


syspage_t *hal_syspage;
addr_t hal_relOffs;


void *hal_syspageRelocate(void *data)
{
	return ((u8 *)data + hal_relOffs);
}


ptr_t hal_syspageAddr(void)
{
	return (ptr_t)hal_syspage;
}


void hal_wdgReload(void)
{
}


int hal_started(void)
{
	return hal_common.started;
}


void _hal_start(void)
{
	hal_common.started = 1;
}


void hal_lockScheduler(void)
{
}


int hal_platformctl(void *ptr)
{
	platformctl_t *pctl = ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&hal_common.pltctlSp, &sc);

	switch (pctl->type) {
		case pctl_reboot:
			if ((pctl->action == pctl_set) && (pctl->task.reboot.magic == PCTL_REBOOT_MAGIC)) {
				hal_cpuReboot();
			}
			break;

		default:
			break;
	}
	hal_spinlockClear(&hal_common.pltctlSp, &sc);

	return ret;
}


__attribute__((section(".init"))) void _hal_init(void)
{
	_hal_sbiInit();
	_hal_spinlockInit();
	_hal_consoleInit();

	_hal_exceptionsInit();
	_hal_interruptsInit();

	_hal_timerInit(SYSTICK_INTERVAL);

#if 0
	_hal_cpuInit();
#endif
	hal_spinlockCreate(&hal_common.pltctlSp, "pltctl");
	hal_common.started = 0;
}
