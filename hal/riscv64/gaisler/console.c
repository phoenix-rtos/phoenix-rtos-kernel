/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console - GRLIB UART
 *
 * Copyright 2022, 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/cpu.h>
#include <arch/pmap.h>

#include "hal/console.h"
#include "hal/types.h"
#include "hal/riscv64/riscv64.h"

#include <board_config.h>

/* UART control bits */
#define TX_EN (1 << 1)

/* UART status bits */
#define TX_FIFO_FULL (1 << 9)


enum {
	uart_data = 0, /* Data register           : 0x00 */
	uart_status,   /* Status register         : 0x04 */
	uart_ctrl,     /* Control register        : 0x08 */
	uart_scaler,   /* Scaler reload register  : 0x0C */
	uart_dbg       /* FIFO debug register     : 0x10 */
};


static struct {
	volatile u32 *uart;
} halconsole_common;


static void _hal_consolePrint(const char *s)
{
	for (; *s; s++) {
		hal_consolePutch(*s);
	}

	/* Wait until TX fifo is empty */
	while ((*(halconsole_common.uart + uart_status) & TX_FIFO_FULL) != 0) {
	}
}


static u32 _hal_consoleCalcScaler(u32 baud)
{
	return (SYSCLK_FREQ / (baud * 8 + 7));
}


void hal_consolePutch(char c)
{
	/* Wait until TX fifo is empty */
	while ((*(halconsole_common.uart + uart_status) & TX_FIFO_FULL) != 0) {
	}
	*(halconsole_common.uart + uart_data) = c;
}


void hal_consolePrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD) {
		_hal_consolePrint(CONSOLE_BOLD);
	}
	else if (attr != ATTR_USER) {
		_hal_consolePrint(CONSOLE_CYAN);
	}

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void _hal_consoleInit(void)
{
	halconsole_common.uart = _pmap_halMapDevice(PAGE_ALIGN(UART0_BASE), PAGE_OFFS(UART0_BASE), SIZE_PAGE);
	*(halconsole_common.uart + uart_ctrl) = 0;

	/* Clear UART FIFO */
	while ((*(halconsole_common.uart + uart_status) & (1 << 0)) != 0) {
		(void)*(halconsole_common.uart + uart_data);
	}
	*(halconsole_common.uart + uart_scaler) = _hal_consoleCalcScaler(UART_BAUDRATE);
	RISCV_FENCE(w, o);
	*(halconsole_common.uart + uart_ctrl) = TX_EN;
}
