/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver (HAL RISCV64)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "interrupts.h"
#include "sbi.h"

#include "../../../include/errno.h"


struct {
	u32 interval;
} timer;


__attribute__ ((section (".init"))) void _timer_init(u32 interval)
{
	cycles_t c = hal_cpuGetCycles2() / 1000;

	timer.interval = interval;

	sbi_call(SBI_SETTIMER, c + 1000L, 0, 0);
	csr_set(sie, SIE_STIE);

	return;
}
