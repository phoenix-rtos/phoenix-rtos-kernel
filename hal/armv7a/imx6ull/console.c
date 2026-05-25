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
#include "hal/pmap.h"

#include <board_config.h>

#ifndef UART_CONSOLE_KERNEL
#define UART_CONSOLE_KERNEL 1
#endif


static struct {
	volatile u32 *base;
	u8 type;
	u32 speed;
} console_common;


/* TODO: add more UARTs - they may require extra configuration, e.g. IOMUX or clock initialization */
static const addr_t console_baseAddr[] = {
	0x02020000U,
	0x021e8000U,
};

#define N_UARTS (sizeof(console_baseAddr) / sizeof(console_baseAddr[0]))

_Static_assert((UART_CONSOLE_KERNEL > 0) && ((size_t)UART_CONSOLE_KERNEL <= N_UARTS), "Invalid UART number selected");


/* clang-format off */
enum { urxd = 0, utxd = 16, ucr1 = 32, ucr2, ucr3, ucr4, ufcr, usr1, usr2,
	uesc, utim, ubir, ubmr, ubrc, onems, uts, umcr };
/* clang-format on */


static void _hal_consolePrint(const char *s)
{
	for (; *s != '\0'; s++) {
		hal_consolePutch(*s);
	}

	while ((*(console_common.base + usr1) & 0x2000U) == 0U) {
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
	while ((*(console_common.base + usr1) & 0x2000U) == 0U) {
		;
	}

	*(console_common.base + utxd) = (unsigned int)c;
}


__attribute__((section(".init"))) void _hal_consoleInit(void)
{
	console_common.base = _pmap_halMapDevice(console_baseAddr[UART_CONSOLE_KERNEL - 1], 0, SIZE_PAGE);
	console_common.speed = 115200;

	*(console_common.base + ucr2) &= ~0x1U;
	while ((*(console_common.base + uts) & 0x1U) != 0U) {
		;
	}

	*(console_common.base + ucr1) = 0x1U;
	*(console_common.base + ucr2) = 0x4026U;
	*(console_common.base + ucr3) = 0x704U;
	*(console_common.base + ucr4) = 0x8000U;
	*(console_common.base + ufcr) = 0x901U;
	*(console_common.base + uesc) = 0x2bU;
	*(console_common.base + utim) = 0x0U;
	*(console_common.base + ubir) = 0x11ffU;
	*(console_common.base + ubmr) = 0xc34fU;
}
