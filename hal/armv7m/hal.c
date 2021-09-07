/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (ARMv7 Cortex-M3)
 *
 * Copyright 2016-2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../hal.h"
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


int hal_started(void)
{
	return hal_common.started;
}


void _hal_start(void)
{
	hal_common.started = 1;
}


void _hal_init(void)
{
	_hal_spinlockInit();
//	_hal_exceptionsInit();
	_hal_interruptsInit();
	_hal_cpuInit();
	_hal_consoleInit();
	_timer_init(SYSTICK_INTERVAL);

	hal_common.started = 0;

	return;
}
