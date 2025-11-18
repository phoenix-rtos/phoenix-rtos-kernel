/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (sparcv8leon)
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"

#ifndef NOMMU
#include <arch/tlb.h>
#endif


static struct {
	int started;
} hal_common;

/* parasoft-begin-suppress MISRAC2012-RULE_8_4 "Definition in assembly" */
/* parasoft-begin-suppress MISRAC2012-RULE_5_8 "Another variable with this name used
 * inside the structure so it shouldn't cause this violation"
 */
syspage_t *syspage;
/* parasoft-end-suppress MISRAC2012-RULE_5_8 */
unsigned int relOffs;
u32 hal_multilock;
/* parasoft-end-suppress MISRAC2012-RULE_8_4 */


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


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void hal_lockScheduler(void)
{
#ifndef NOMMU
	hal_tlbShootdown();
#endif

	/* clang-format off */

	__asm__ volatile (
	".align 16\n\t" /* GRLIB TN-0011 errata */
	"1: \n\t"
		"ldstub [%0], %%g2\n\t"
		"tst %%g2\n\t"
		"be 3f\n\t"
		"nop\n\t"
	"2: \n\t"
		"ldub [%0], %%g2\n\t"
		"tst %%g2\n\t"
		"bne 2b\n\t"
		"nop\n\t"
		"ba,a 1b\n\t"
	"3: \n\t"
		"nop\n\t"
	:
	: "r"(&hal_multilock)
	: "g2", "memory", "cc"
	);

	/* clang-format on */
}


void _hal_init(void)
{
	hal_common.started = 0;
	hal_multilock = 0U;

	_hal_spinlockInit();
	_hal_exceptionsInit();
	_pmap_halInit();
	_hal_interruptsInit();
	_hal_platformInit();
	_hal_cpuInit();
	_hal_consoleInit();
	_hal_timerInit(SYSTICK_INTERVAL);
}
