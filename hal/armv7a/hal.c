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

#include "../hal.h"

#include "halsyspage.h"

struct {
	int started;
} hal_common;

syspage_t *syspage;
unsigned int relOffs;


extern void _hal_platformInit(void);


void *hal_syspageRelocate(void *data)
{
	return ((u8 *)data + relOffs);
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


__attribute__ ((section (".init"))) void _hal_init(void)
{
	_hal_spinlockInit();
	_hal_platformInit();
	_hal_consoleInit();

	_hal_exceptionsInit();
	_hal_interruptsInit();

	_hal_timerInit(SYSTICK_INTERVAL);

	hal_common.started = 0;
	return;
}
