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

static struct {
	volatile u32 *uart1;
	volatile u32 *uart2;
	u8 type;
	u32 speed;
} console_common;


/* clang-format off */
enum { urxd = 0, utxd = 16, ucr1 = 32, ucr2, ucr3, ucr4, ufcr, usr1, usr2,
	uesc, utim, ubir, ubmr, ubrc, onems, uts, umcr };
/* clang-format on */


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _end;


static void _hal_consolePrint(const char *s)
{
	for (; *s != '\0'; s++) {
		hal_consolePutch(*s);
	}

	while ((*(console_common.UART + usr1) & 0x2000U) == 0U) {
		;
	}
}


void hal_consolePrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD) {
		_hal_consolePrint(CONSOLE_BOLD);
	}
	else if (attr != ATTR_USER) {
		_hal_consolePrint(CONSOLE_CYAN);
	}
	else {
		/* No action required */
	}

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void hal_consolePutch(char c)
{
	/* Wait for transmitter readiness */
	while ((*(console_common.UART + usr1) & 0x2000U) == 0U) {
		;
	}

	*(console_common.UART + utxd) = (unsigned int)c;
}


__attribute__((section(".init"))) void _hal_consoleInit(void)
{
	console_common.uart1 = (void *)(((u32)&_end + (3U * SIZE_PAGE) - 1U) & ~(SIZE_PAGE - 1U));
	console_common.uart2 = (void *)(((u32)&_end + (4U * SIZE_PAGE) - 1U) & ~(SIZE_PAGE - 1U));
	console_common.speed = 115200;

	*(console_common.UART + ucr2) &= ~0x1U;
	while ((*(console_common.UART + uts) & 0x1U) != 0U) {
		;
	}

	*(console_common.UART + ucr1) = 0x1U;
	*(console_common.UART + ucr2) = 0x4026U;
	*(console_common.UART + ucr3) = 0x704U;
	*(console_common.UART + ucr4) = 0x8000U;
	*(console_common.UART + ufcr) = 0x901U;
	*(console_common.UART + uesc) = 0x2bU;
	*(console_common.UART + utim) = 0x0U;
	*(console_common.UART + ubir) = 0x11ffU;
	*(console_common.UART + ubmr) = 0xc34fU;
}
