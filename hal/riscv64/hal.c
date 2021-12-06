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

#include "../hal.h"
#include "../spinlock.h"
#include "../console.h"
#include "../exceptions.h"
#include "../interrupts.h"
#include "../cpu.h"
#include "../pmap.h"
#include "../timer.h"
#include "config.h"


struct {
	int started;
} hal_common;

hal_syspage_t *syspage;


void *hal_syspageRelocate(void *data)
{
	return data;
}


ptr_t hal_syspageAddr(void)
{
	return (ptr_t)syspage;
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


__attribute__ ((section (".init"))) void _hal_init(void)
{
	_hal_spinlockInit();
	_hal_consoleInit();

	_hal_exceptionsInit();
	_hal_interruptsInit();

	_hal_timerInit(SYSTICK_INTERVAL);

#if 0
	_hal_cpuInit();
#endif
	hal_common.started = 0;

	return;
}
