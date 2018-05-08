/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (ANSI TTY via IMX UART2)
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

#define UART uart1

struct {
	volatile u32 *uart1;
	volatile u32 *uart2;
	u8 type;
	u32 speed;
} console_common;

extern void _end(void);

enum { urxd = 0, utxd = 16, ucr1 = 32, ucr2, ucr3, ucr4, ufcr, usr1, usr2,
	uesc, utim, ubir, ubmr, ubrc, onems, uts, umcr };


static void _console_print(const char *s)
{
	for (; *s != NULL; s++) {
		/* Wait for transmitter readiness */
		while (!(*(console_common.UART + usr1) & 0x2000));

		*(console_common.UART + utxd) = *s;
	}

	while (!(*(console_common.UART + usr1) & 0x2000));
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

	console_common.uart1 = (void *)(((u32)_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	console_common.uart2 = (void *)(((u32)_end + (2 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	console_common.speed = 115200;

	_console_print("\033[2J");
	_console_print("\033[0;0f");
}
