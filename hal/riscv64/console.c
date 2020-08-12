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

#include "console.h"
#include "cpu.h"
#include "syspage.h"
#include "spinlock.h"
#include "sbi.h"


struct {
	spinlock_t spinlock;
} console_common;


void _console_print(const char *s)
{

	for (; *s; s++)
		sbi_ecall(1, 0, *s, 0, 0, 0, 0, 0);
}


void hal_consolePrint(int attr, const char *s)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&console_common.spinlock, &sc);

	if (attr == ATTR_BOLD) {
		_console_print("\033[1m");
		_console_print(s);
		_console_print("\033[0m");
	}
	else if (attr != ATTR_USER) {
		_console_print("\033[36m");
		_console_print(s);
		_console_print("\033[0m");
	}
	else
		_console_print(s);

	hal_spinlockClear(&console_common.spinlock, &sc);
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{
	hal_spinlockCreate(&console_common.spinlock, "console.spinlock");

	return;
}
