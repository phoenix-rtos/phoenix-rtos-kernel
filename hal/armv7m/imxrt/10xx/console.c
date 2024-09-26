/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (iMXRT USART + RTT)
 *
 * Copyright 2016-2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/console.h"
#include "hal/arm/rtt.h"
#include "include/arch/armv7m/imxrt/10xx/imxrt10xx.h"
#include "imxrt10xx.h"
#include "lib/helpers.h"
#include <arch/cpu.h>

#include <board_config.h>

#ifndef UART_CONSOLE_KERNEL
#ifdef UART_CONSOLE
#define UART_CONSOLE_KERNEL UART_CONSOLE
#else
#define UART_CONSOLE_KERNEL 1
#endif
#endif

#ifndef RTT_CONSOLE_KERNEL
#if RTT_ENABLED
#define RTT_CONSOLE_KERNEL 0
#else
#define RTT_CONSOLE_KERNEL
#endif
#endif

#define CONCAT3(a, b, c) a##b##c
#define CONSOLE_BAUD(n)  (CONCAT3(UART, n, _BAUDRATE))

#if !ISEMPTY(UART_CONSOLE_KERNEL) && CONSOLE_BAUD(UART_CONSOLE_KERNEL)
#define CONSOLE_BAUDRATE CONSOLE_BAUD(UART_CONSOLE_KERNEL)
#else
#define CONSOLE_BAUDRATE 115200
#endif

struct {
	volatile u32 *uart;
} console_common;


enum { uart_verid = 0, uart_param, uart_global, uart_pincfg, uart_baud, uart_stat, uart_ctrl,
	uart_data, uart_match, uart_modir, uart_fifo, uart_water };


static void _hal_consolePrint(const char *s)
{
	while (*s)
		hal_consolePutch(*(s++));
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
#if RTT_ENABLED && !ISEMPTY(RTT_CONSOLE_KERNEL)
	_hal_rttWrite(RTT_CONSOLE_KERNEL, &c, 1);
#endif

#if !ISEMPTY(UART_CONSOLE_KERNEL)
	while (!(*(console_common.uart + uart_stat) & (1 << 23)))
		;

	*(console_common.uart + uart_data) = c;
#endif
}


#if !ISEMPTY(UART_CONSOLE_KERNEL)
static void _hal_uartInit(void)
{
	u32 t, console = UART_CONSOLE_KERNEL - 1;

	static const struct {
		volatile u32 *base;
		int mode;
		int clk;
		int txmux, txpad;
		int rxmux, rxpad;
		int txdaisy, txsel;
		int rxdaisy, rxsel;
	} info[] = {
		{ (void *)0x40184000, 2, pctl_clk_lpuart1, pctl_mux_gpio_ad_b0_12, pctl_mux_gpio_ad_b0_12, pctl_mux_gpio_ad_b0_13, pctl_mux_gpio_ad_b0_13, -1, -1, -1, -1 },
		{ (void *)0x40188000, 2, pctl_clk_lpuart2, pctl_mux_gpio_ad_b1_02, pctl_mux_gpio_ad_b1_02, pctl_mux_gpio_ad_b1_03, pctl_mux_gpio_ad_b1_03, pctl_isel_lpuart2_tx, 1, pctl_isel_lpuart2_rx, 1 },
		{ (void *)0x4018c000, 2, pctl_clk_lpuart3, pctl_mux_gpio_emc_13, pctl_mux_gpio_emc_13, pctl_mux_gpio_emc_14, pctl_mux_gpio_emc_14, pctl_isel_lpuart3_tx, 1, pctl_isel_lpuart3_rx, 1 },
		{ (void *)0x40190000, 2, pctl_clk_lpuart4, pctl_mux_gpio_emc_19, pctl_mux_gpio_emc_19, pctl_mux_gpio_emc_20, pctl_mux_gpio_emc_20, pctl_isel_lpuart4_tx, 1, pctl_isel_lpuart4_rx, 1 },
		{ (void *)0x40194000, 2, pctl_clk_lpuart5, pctl_mux_gpio_emc_23, pctl_mux_gpio_emc_23, pctl_mux_gpio_emc_24, pctl_mux_gpio_emc_24, pctl_isel_lpuart5_tx, 0, pctl_isel_lpuart5_rx, 0 },
		{ (void *)0x40198000, 2, pctl_clk_lpuart6, pctl_mux_gpio_emc_25, pctl_mux_gpio_emc_25, pctl_mux_gpio_emc_26, pctl_mux_gpio_emc_26, pctl_isel_lpuart6_tx, 0, pctl_isel_lpuart6_rx, 0 },
		{ (void *)0x4019c000, 2, pctl_clk_lpuart7, pctl_mux_gpio_emc_31, pctl_mux_gpio_emc_31, pctl_mux_gpio_emc_32, pctl_mux_gpio_emc_32, pctl_isel_lpuart7_tx, 1, pctl_isel_lpuart7_rx, 1 },
		{ (void *)0x401a0000, 2, pctl_clk_lpuart8, pctl_mux_gpio_emc_28, pctl_mux_gpio_emc_38, pctl_mux_gpio_emc_39, pctl_mux_gpio_emc_39, pctl_isel_lpuart8_tx, 2, pctl_isel_lpuart8_rx, 2 },
	};

	console_common.uart = info[console].base;

	_imxrt_ccmControlGate(info[console].clk, clk_state_run_wait);

	/* tx */
	_imxrt_setIOmux(info[console].txmux, 0, info[console].mode);
	_imxrt_setIOpad(info[console].txpad, 0, 0, 0, 1, 0, 2, 6, 0);

	if (info[console].txdaisy >= 0)
		_imxrt_setIOisel(info[console].txdaisy, info[console].txsel);

	/* rx */
	_imxrt_setIOmux(info[console].rxmux, 0, info[console].mode);
	_imxrt_setIOpad(info[console].rxpad, 0, 0, 0, 1, 0, 2, 6, 0);

	if (info[console].rxdaisy >= 0)
		_imxrt_setIOisel(info[console].rxdaisy, info[console].rxsel);

	_imxrt_ccmSetMux(clk_mux_uart, 0);
	_imxrt_ccmSetDiv(clk_div_uart, 0);

	/* Reset all internal logic and registers, except the Global Register */
	*(console_common.uart + uart_global) |= 1 << 1;
	hal_cpuDataMemoryBarrier();
	*(console_common.uart + uart_global) &= ~(1 << 1);
	hal_cpuDataMemoryBarrier();

	/* Set baud rate */
	t = *(console_common.uart + uart_baud) & ~((0x1f << 24) | (1 << 17) | 0xfff);

	/* For baud rate calculation, default UART_CLK=80MHz assumed */
	switch (CONSOLE_BAUDRATE) {
		case 9600: t |= 0x0c000281; break;
		case 19200: t |= 0x080001cf; break;
		case 38400: t |= 0x03020209; break;
		case 57600: t |= 0x0302015b; break;
		case 115200: t |= 0x0402008b; break;
		case 230400: t |= 0x1c00000c; break;
		case 460800: t |= 0x1c000006; break;
		/* As fallback use default 115200 */
		default: t |= 0x0402008b; break;
	}
	*(console_common.uart + uart_baud) = t;

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
#endif


void _hal_consoleInit(void)
{
#if RTT_ENABLED && !ISEMPTY(RTT_CONSOLE_KERNEL)
	_hal_rttInit();
#endif

#if !ISEMPTY(UART_CONSOLE_KERNEL)
	_hal_uartInit();
#endif
}
