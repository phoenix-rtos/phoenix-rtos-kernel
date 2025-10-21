/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (MCXN94x UART)
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "mcxn94x.h"

#include "hal/console.h"
#include "hal/cpu.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include <board_config.h>


/* clang-format off */
enum { uart_verid = 0, uart_param, uart_global, uart_pincfg, uart_baud,
	uart_stat, uart_ctrl, uart_data, uart_match, uart_modir, uart_fifo,
	uart_water, uart_dataro, uart_mcr = 16, uart_msr, uart_reir, uart_teir,
	uart_hdcr, uart_tocr = 22, uart_tosr, uart_timeoutn, uart_tcbrn = 128,
	uart_tdbrn = 256 };
/* clang-format on */


static struct {
	volatile u32 *uart;
} console_common;


static void _hal_consolePrint(const char *s)
{
	while (*s != '\0') {
		hal_consolePutch(*(s++));
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
	while ((*(console_common.uart + uart_stat) & (1UL << 23)) == 0U) {
	}

	*(console_common.uart + uart_data) = (u32)c;
}


void _hal_consoleInit(void)
{
	u32 t;
	static const struct {
		volatile u32 *base;
		int tx;
		int rx;
		int txalt;
		int rxalt;
	} info[10] = {
		{ .base = FLEXCOMM0_BASE, .tx = UART0_TX_PIN, .rx = UART0_RX_PIN, .txalt = UART0_TX_ALT, .rxalt = UART0_RX_ALT },
		{ .base = FLEXCOMM1_BASE, .tx = UART1_TX_PIN, .rx = UART1_RX_PIN, .txalt = UART1_TX_ALT, .rxalt = UART1_RX_ALT },
		{ .base = FLEXCOMM2_BASE, .tx = UART2_TX_PIN, .rx = UART2_RX_PIN, .txalt = UART2_TX_ALT, .rxalt = UART2_RX_ALT },
		{ .base = FLEXCOMM3_BASE, .tx = UART3_TX_PIN, .rx = UART3_RX_PIN, .txalt = UART3_TX_ALT, .rxalt = UART3_RX_ALT },
		{ .base = FLEXCOMM4_BASE, .tx = UART4_TX_PIN, .rx = UART4_RX_PIN, .txalt = UART4_TX_ALT, .rxalt = UART4_RX_ALT },
		{ .base = FLEXCOMM5_BASE, .tx = UART5_TX_PIN, .rx = UART5_RX_PIN, .txalt = UART5_TX_ALT, .rxalt = UART5_RX_ALT },
		{ .base = FLEXCOMM6_BASE, .tx = UART6_TX_PIN, .rx = UART6_RX_PIN, .txalt = UART6_TX_ALT, .rxalt = UART6_RX_ALT },
		{ .base = FLEXCOMM7_BASE, .tx = UART7_TX_PIN, .rx = UART7_RX_PIN, .txalt = UART7_TX_ALT, .rxalt = UART7_RX_ALT },
		{ .base = FLEXCOMM8_BASE, .tx = UART8_TX_PIN, .rx = UART8_RX_PIN, .txalt = UART8_TX_ALT, .rxalt = UART8_RX_ALT },
		{ .base = FLEXCOMM9_BASE, .tx = UART9_TX_PIN, .rx = UART9_RX_PIN, .txalt = UART9_TX_ALT, .rxalt = UART9_RX_ALT },
	};
	static const int baud[10] = { UART0_BAUDRATE, UART1_BAUDRATE, UART2_BAUDRATE, UART3_BAUDRATE, UART4_BAUDRATE,
		UART5_BAUDRATE, UART6_BAUDRATE, UART7_BAUDRATE, UART8_BAUDRATE, UART9_BAUDRATE };

	console_common.uart = info[UART_CONSOLE].base;

	/* Configure RX and TX pins */
	(void)_mcxn94x_portPinConfig(info[UART_CONSOLE].rx, info[UART_CONSOLE].rxalt, (int)(MCX_PIN_SLOW | MCX_PIN_WEAK | MCX_PIN_PULLUP_WEAK | MCX_PIN_INPUT_BUFFER_ENABLE));
	(void)_mcxn94x_portPinConfig(info[UART_CONSOLE].tx, info[UART_CONSOLE].txalt, (int)(MCX_PIN_SLOW | MCX_PIN_WEAK));

	/* Reset all internal logic and registers, except the Global Register */
	*(console_common.uart + uart_global) |= 1U << 1;
	hal_cpuDataMemoryBarrier();
	*(console_common.uart + uart_global) &= ~(1U << 1);
	hal_cpuDataMemoryBarrier();

	/* Set baud rate */
	t = *(console_common.uart + uart_baud) & ~((0xfUL << 24) | (1UL << 17) | 0x1fffU);

	/* For baud rate calculation, default UART_CLK=12MHz assumed */
	switch (baud[UART_CONSOLE]) {
		case 9600: t |= 0x03020138U; break;
		case 19200: t |= 0x0302009cU; break;
		case 38400: t |= 0x0302004eU; break;
		case 57600: t |= 0x03020034U; break;
		case 230400: t |= 0x0302000dU; break;
		default: t |= 0x0302001aU; break; /* 115200 */
	}
	*(console_common.uart + uart_baud) = t;

	/* Set 8 bit and no parity mode */
	*(console_common.uart + uart_ctrl) &= ~0x117U;

	/* One stop bit */
	*(console_common.uart + uart_baud) &= ~(1UL << 13);

	*(console_common.uart + uart_water) = 0;

	/* Enable FIFO */
	*(console_common.uart + uart_fifo) |= (1U << 7) | (1U << 3);
	*(console_common.uart + uart_fifo) |= 0x3UL << 14;

	/* Clear all status flags */
	*(console_common.uart + uart_stat) |= 0xc01fc000U;

	/* Enable TX and RX */
	*(console_common.uart + uart_ctrl) |= (1UL << 19) | (1UL << 18);
}
