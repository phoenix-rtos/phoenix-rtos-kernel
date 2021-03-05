/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (ARM)
 *
 * Copyright 2014, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "spinlock.h"
#include "console.h"
#include "exceptions.h"
#include "interrupts.h"
#include "cpu.h"
#include "timer.h"


struct {
	int started;
} hal_common;


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
	_hal_platformInit();
	_hal_consoleInit();

	_hal_exceptionsInit();
	_hal_interruptsInit();

	_timer_init(SYSTICK_INTERVAL);
	_hal_cpuInit();

	hal_common.started = 0;
	return;
}
