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

#include "hal/console.h"
#include "hal/cpu.h"

#define UART uart1

struct {
	volatile u32 *uart1;
	volatile u32 *uart2;
	u8 type;
	u32 speed;
} console_common;


enum { urxd = 0, utxd = 16, ucr1 = 32, ucr2, ucr3, ucr4, ufcr, usr1, usr2,
	uesc, utim, ubir, ubmr, ubrc, onems, uts, umcr };


extern unsigned int _end;


static void _hal_consolePrint(const char *s)
{
	for (; *s; s++)
		hal_consolePutch(*s);

	while (!(*(console_common.UART + usr1) & 0x2000))
		;
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
	/* Wait for transmitter readiness */
	while (!(*(console_common.UART + usr1) & 0x2000))
		;

	*(console_common.UART + utxd) = c;
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{
	console_common.uart1 = (void *)(((u32)&_end + (3 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	console_common.uart2 = (void *)(((u32)&_end + (4 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	console_common.speed = 115200;

	*(console_common.UART + ucr2) &= ~0x1;
	while (*(console_common.UART + uts) & 0x1);

	*(console_common.UART + ucr1) = 0x1;
	*(console_common.UART + ucr2) = 0x4026;
	*(console_common.UART + ucr3) = 0x704;
	*(console_common.UART + ucr4) = 0x8000;
	*(console_common.UART + ufcr) = 0x901;
	*(console_common.UART + uesc) = 0x2b;
	*(console_common.UART + utim) = 0x0;
	*(console_common.UART + ubir) = 0x11ff;
	*(console_common.UART + ubmr) = 0xc34f;
}
