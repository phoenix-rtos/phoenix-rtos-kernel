/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (ARMv8M)
 *
 * Copyright 2016-2017, 2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
struct {
	int started;
} hal_common;


hal_syspage_t *syspage;


extern void _hal_cpuInit(void);


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


void hal_lockScheduler(void)
{
}


void _hal_init(void)
{
	hal_common.started = 0;

	_hal_spinlockInit();
	_hal_exceptionsInit();
	_hal_interruptsInit();
	_hal_cpuInit();
	_hal_consoleInit();
	_hal_timerInit(SYSTICK_INTERVAL);
}
