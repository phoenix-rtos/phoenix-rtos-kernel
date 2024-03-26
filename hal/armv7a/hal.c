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

#include "hal/hal.h"

#include "halsyspage.h"

struct {
	int started;
} hal_common;

syspage_t *syspage;
unsigned int relOffs;
u32 schedulerLocked = 0;


extern void _hal_platformInit(void);
extern void _hal_cpuInit(void);


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
#if NUM_CPUS != 1
	/* clang-format off */
	__asm__ volatile(
	"1:\n"
		"dmb\n"
		"mov r2, #1\n"
		"ldrex r1, [%0]\n"
		"cmp r1, #0\n"
		"bne 1b\n"
		"strex r1, r2, [%0]\n"
		"cmp r1, #0\n"
		"bne 1b\n"
		"dmb\n"
	:
	: "r" (&schedulerLocked)
	: "r1", "r2", "memory", "cc");
	/* clang-format on */
#else
	/* Not necessary on single-core systems */
	(void)schedulerLocked;
	return;
#endif
}


__attribute__ ((section (".init"))) void _hal_init(void)
{
	schedulerLocked = 0;
	_hal_spinlockInit();
	_hal_platformInit();
	_hal_consoleInit();

	_hal_exceptionsInit();
	_hal_interruptsInit();

	_hal_cpuInit();

	_hal_timerInit(SYSTICK_INTERVAL);

	hal_common.started = 0;
	return;
}
