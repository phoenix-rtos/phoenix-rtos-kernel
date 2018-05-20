/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (via SBI)
 *
 * Copyright 2018 Phoenix Systems
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
	spinlock_t lock;
} console_common;


static void _console_print(const char *s)
{

	for (; *s; s++)
		sbi_call(1, *s, 0, 0);
}


void hal_consolePrint(int attr, const char *s)
{
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
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{

	return;
}
