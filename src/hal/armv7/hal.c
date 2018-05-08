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

#include "syspage.h"
#include "spinlock.h"
#include "console.h"
#include "interrupts.h"
#include "spinlock.h"
#include "timer.h"
#include "cpu.h"


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


void _hal_init(void)
{
	_hal_spinlockInit();
//	_hal_exceptionsInit();
	_hal_interruptsInit();
	_hal_cpuInit();
	_hal_consoleInit();
	_timer_init(1000);

	hal_common.started = 0;

	return;
}
