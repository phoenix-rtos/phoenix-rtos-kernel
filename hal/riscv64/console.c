/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (via SBI)
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../console.h"
#include "../spinlock.h"
#include "sbi.h"


struct {
	spinlock_t spinlock;
} console_common;


void _hal_consolePrint(const char *s)
{
	for (; *s; s++)
		hal_consolePutch(*s);
}


void hal_consolePrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD)
		_hal_consolePrint(CONSOLE_BOLD);
	else if (attr != ATTR_USER)
		_hal_consolePrint(CONSOLE_CYAN);

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void hal_consolePutch(char c)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&console_common.spinlock, &sc);
	sbi_ecall(1, 0, c, 0, 0, 0, 0, 0);
	hal_spinlockClear(&console_common.spinlock, &sc);
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{
	hal_spinlockCreate(&console_common.spinlock, "console.spinlock");

	return;
}
