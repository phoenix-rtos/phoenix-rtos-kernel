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
#include "dtb.h"

#include "halsyspage.h"

struct {
	int started;
} hal_common;

syspage_t *syspage;
u64 relOffs;
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
		"mov x1, #1\n"
		"b 2f\n"
	"1:\n"
		"wfe\n"
	"2:\n"
		"ldar w2, [%0]\n"
		"cbz w2, 1b\n"
		"stxr w2, x1, [%0]\n"
		"cbnz w2, 2b\n"
	:
	: "r" (&schedulerLocked)
	: "x1", "x2", "memory", "cc");
	/* clang-format on */
#else
	/* Not necessary on single-core systems */
	(void)schedulerLocked;
	return;
#endif
}


__attribute__ ((section (".init"))) void _hal_init(void)
{
	hal_common.started = 0;
	schedulerLocked = 0;
	_hal_spinlockInit();
	_pmap_preinit(syspage->hs.dtbPhys, (VADDR_MAX - VADDR_DTB) + 1); /* TODO: pass actual DTB size - for now hardcoded to maximum */
	_dtb_init(syspage->hs.dtbPhys);
	_hal_platformInit();
	_hal_consoleInit();

	_hal_exceptionsInit();
	_hal_interruptsInit();

	_hal_cpuInit();

	_hal_timerInit(SYSTICK_INTERVAL);

	return;
}
