/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (ARM)
 *
 * Copyright 2014, 2018, 2024, 2025 Phoenix Systems
 * Author: Pawel Pisarczyk, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"

#include "halsyspage.h"


static struct {
	int started;
} hal_common;


syspage_t *syspage;
u32 relOffs;
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
	__asm__ volatile (
	"1:\n\t"
		"dmb\n\t"
		"mov r2, #1\n\t"
		"ldrex r1, [%0]\n\t"
		"cmp r1, #0\n\t"
		"bne 1b\n\t"
		"strex r1, r2, [%0]\n\t"
		"cmp r1, #0\n\t"
		"bne 1b\n\t"
		"dmb"
	:
	: "r" (&schedulerLocked)
	: "r1", "r2", "memory", "cc");
	/* clang-format on */
#else
	/* Not necessary on single-core systems */
	(void)schedulerLocked;
#endif
}


__attribute__((section(".init"))) void _hal_init(void)
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
}
