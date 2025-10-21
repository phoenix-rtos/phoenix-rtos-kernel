/*
 * Phoenix-RTOS
 *
 * Operating system loader
 *
 * Console
 *
 * Copyright 2024 Phoenix Systems
 * Authors: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <hal/hal.h>

#include <board_config.h>

#include "hal/armv8r/armv8r.h"


#define TX_BUF_FULL (1U << 0)

#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)

#define UART_CONSOLE_BASE CONCAT(UART, CONCAT(UART_CONSOLE_KERNEL, _BASE))


static struct {
	volatile u32 *uart;
} halconsole_common;


/* UART registers */
/* clang-format off */
enum { data = 0, state, ctrl, intstatus, bauddiv };
/* clang-format on */


void hal_consolePutch(char c)
{
	/* No hardware FIFO, wait until TX buffer is empty */
	while ((*(halconsole_common.uart + state) & TX_BUF_FULL) != 0U) {
	}
	*(halconsole_common.uart + data) = (u32)c;
}


static void _hal_consolePrint(const char *s)
{
	const char *ptr;

	for (ptr = s; *ptr != '\0'; ++ptr) {
		hal_consolePutch(*ptr);
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


void _hal_consoleInit(void)
{
	/* Set scaler */
	u32 scaler = (u32)SYSCLK_FREQ / (u32)UART_BAUDRATE;
	halconsole_common.uart = (void *)UART_CONSOLE_BASE;
	*(halconsole_common.uart + bauddiv) = scaler;
	hal_cpuDataSyncBarrier();

	/* Enable TX */
	*(halconsole_common.uart + ctrl) = 0x1U;
}
