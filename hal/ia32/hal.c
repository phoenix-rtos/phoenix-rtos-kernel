/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (IA32 PC)
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../spinlock.h"
#include "../console.h"
#include "../exceptions.h"
#include "../interrupts.h"
#include "../cpu.h"
#include "../pmap.h"
#include "../timer.h"
#include "pci.h"
#include "halsyspage.h"


struct {
	int started;
} hal_common;


syspage_t *syspage;


extern void _hal_cpuInit(void);


void *hal_syspageRelocate(void *data)
{
	return ((u8 *)data + VADDR_KERNEL);
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


void hal_wdgReload(void)
{
}


__attribute__ ((section (".init"))) void _hal_init(void)
{

	_hal_spinlockInit();
	_hal_consoleInit();


	_hal_exceptionsInit();
	_hal_interruptsInit();

	_hal_timerInit(SYSTICK_INTERVAL);
	_hal_cpuInit();
	_hal_pciInit();

	hal_common.started = 0;
	return;
}
