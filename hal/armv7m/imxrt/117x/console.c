/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (i.MX RT1170 UART)
 *
 * Copyright 2016-2017, 2019 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "console.h"
#include "imxrt117x.h"
#include "../../include/errno.h"
#include "../../include/arch/imxrt1170.h"

#ifndef CONSOLE
#define CONSOLE 11
#endif

struct {
	volatile u32 *uart;
} console_common;


enum { uart_verid = 0, uart_param, uart_global, uart_pincfg, uart_baud, uart_stat, uart_ctrl,
	uart_data, uart_match, uart_modir, uart_fifo, uart_water };


void _hal_consolePrint(const char *s)
{
	while (*s) {
		while (!(*(console_common.uart + uart_stat) & (1 << 23)));
		*(console_common.uart + uart_data) = *(s++);
	}
}


void hal_consolePrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD)
		_hal_consolePrint("\033[1m");
	_hal_consolePrint(s);
	if (attr == ATTR_BOLD)
		_hal_consolePrint("\033[0m");
	return;
}


void _hal_consoleInit(void)
{
	u32 t, console = CONSOLE -1;

	static const struct {
		volatile u32 *base;
		int dev;
	} info[] = {
		{ ((void *)0x4007c000), pctl_clk_lpuart1 },
		{ ((void *)0x40080000), pctl_clk_lpuart2 },
		{ ((void *)0x40084000), pctl_clk_lpuart3 },
		{ ((void *)0x40088000), pctl_clk_lpuart4 },
		{ ((void *)0x4008c000), pctl_clk_lpuart5 },
		{ ((void *)0x40090000), pctl_clk_lpuart6 },
		{ ((void *)0x40094000), pctl_clk_lpuart7 },
		{ ((void *)0x40098000), pctl_clk_lpuart8 },
		{ ((void *)0x4009c000), pctl_clk_lpuart9 },
		{ ((void *)0x400a0000), pctl_clk_lpuart10 },
		{ ((void *)0x40c24000), pctl_clk_lpuart11 },
		{ ((void *)0x40c28000), pctl_clk_lpuart12 }
	};

	console_common.uart = info[console].base;

	_imxrt_setDevClock(info[console].dev, 0, 0, 0, 0, 1);

	/* tx */
	_imxrt_setIOmux(pctl_mux_gpio_ad_24, 0, 0);
	_imxrt_setIOpad(pctl_pad_gpio_ad_24, 0, 0, 0, 0, 0, 0);

	/* rx */
	_imxrt_setIOmux(pctl_mux_gpio_ad_25, 0, 0);
	_imxrt_setIOpad(pctl_pad_gpio_ad_25, 0, 0, 1, 1, 0, 0);

	_imxrt_setIOisel(pctl_isel_lpuart1_rxd, 0);
	_imxrt_setIOisel(pctl_isel_lpuart1_txd, 0);

	/* Reset all internal logic and registers, except the Global Register */
	*(console_common.uart + uart_global) |= 1 << 1;
	hal_cpuDataBarrier();
	*(console_common.uart + uart_global) &= ~(1 << 1);
	hal_cpuDataBarrier();

	/* Set 115200 baudrate */
	t = *(console_common.uart + uart_baud) & ~((0x1f << 24) | (1 << 17) | 0xfff);
	*(console_common.uart + uart_baud) = t | 50462772;

	/* Set 8 bit and no parity mode */
	*(console_common.uart + uart_ctrl) &= ~0x117;

	/* One stop bit */
	*(console_common.uart + uart_baud) &= ~(1 << 13);

	*(console_common.uart + uart_water) = 0;

	/* Enable FIFO */
	*(console_common.uart + uart_fifo) |= (1 << 7) | (1 << 3);
	*(console_common.uart + uart_fifo) |= 0x3 << 14;

	/* Clear all status flags */
	*(console_common.uart + uart_stat) |= 0xc01fc000;

	/* Enable TX and RX */
	*(console_common.uart + uart_ctrl) |= (1 << 19) | (1 << 18);
}
