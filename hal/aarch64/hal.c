/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (AArch64)
 *
 * Copyright 2014, 2018, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "dtb.h"

#include "syspage.h"
#include "halsyspage.h"

struct {
	int started;
} hal_common;

syspage_t *syspage;
u64 relOffs;
u32 schedulerLocked = 0;


void _hal_cpuInit(void);


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
		"mov w1, #1\n"
		"b 2f\n"
	"1:\n"
		"wfe\n"
	"2:\n"
		"ldaxr w2, [%0]\n"
		"cbnz w2, 1b\n"
		"stxr w2, w1, [%0]\n"
		"cbnz w2, 2b\n"
	:
	: "r" (&schedulerLocked)
	: "x1", "x2", "memory");
	/* clang-format on */
#else
	/* Not necessary on single-core systems */
	(void)schedulerLocked;
	return;
#endif
}


__attribute__((section(".init"))) void _hal_init(void)
{
	const syspage_prog_t *dtb;
	hal_common.started = 0;
	schedulerLocked = 0;
	_hal_spinlockInit();
	dtb = syspage_progNameResolve("system.dtb");
	_pmap_preinit(dtb->start, dtb->end);
	_hal_platformInit();
	_hal_consoleInit();

	_hal_exceptionsInit();
	_hal_interruptsInit();

	_hal_cpuInit();

	_hal_timerInit(SYSTICK_INTERVAL);

	return;
}
