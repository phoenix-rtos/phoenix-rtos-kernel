/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (i.MX RT1170 UART + RTT)
 *
 * Copyright 2016-2017, 2019 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/console.h"
#include "hal/arm/rtt.h"
#include "include/arch/armv7m/imxrt/11xx/imxrt1170.h"
#include "imxrt117x.h"
#include "lib/helpers.h"
#include <arch/cpu.h>

#include <board_config.h>

#ifndef UART_CONSOLE_KERNEL
#ifdef UART_CONSOLE
#define UART_CONSOLE_KERNEL UART_CONSOLE
#else
#define UART_CONSOLE_KERNEL 11
#endif
#endif

#ifndef RTT_ENABLED
#define RTT_ENABLED 0
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
		{ (void *)0x4007c000, 0, pctl_clk_lpuart1, pctl_mux_gpio_ad_24, pctl_pad_gpio_ad_24, pctl_mux_gpio_ad_25, pctl_pad_gpio_ad_25, pctl_isel_lpuart1_txd, 0, pctl_isel_lpuart1_rxd, 0 },
		{ (void *)0x40080000, 2, pctl_clk_lpuart2, pctl_mux_gpio_disp_b2_10, pctl_pad_gpio_disp_b2_10, pctl_mux_gpio_disp_b2_11, pctl_pad_gpio_disp_b2_11, -1, -1, -1, -1 },
		{ (void *)0x40084000, 4, pctl_clk_lpuart3, pctl_mux_gpio_ad_30, pctl_pad_gpio_ad_30, pctl_mux_gpio_ad_31, pctl_pad_gpio_ad_31, -1, -1, -1, -1 },
		{ (void *)0x40088000, 2, pctl_clk_lpuart4, pctl_mux_gpio_disp_b1_06, pctl_pad_gpio_disp_b1_06, pctl_mux_gpio_disp_b1_04, pctl_pad_gpio_disp_b1_04, -1, -1, -1, -1 },
		{ (void *)0x4008c000, 1, pctl_clk_lpuart5, pctl_mux_gpio_ad_28, pctl_pad_gpio_ad_28, pctl_mux_gpio_ad_29, pctl_pad_gpio_ad_29, -1, -1, -1, -1 },
		{ (void *)0x40090000, 3, pctl_clk_lpuart6, pctl_mux_gpio_emc_b1_40, pctl_pad_gpio_emc_b1_40, pctl_mux_gpio_emc_b1_41, pctl_pad_gpio_emc_b1_41, -1, -1, -1, -1 },
		{ (void *)0x40094000, 2, pctl_clk_lpuart7, pctl_mux_gpio_disp_b2_06, pctl_pad_gpio_disp_b2_06, pctl_mux_gpio_disp_b2_07, pctl_pad_gpio_disp_b2_07, pctl_isel_lpuart7_txd, 1, pctl_isel_lpuart7_rxd, 1 },
		{ (void *)0x40098000, 2, pctl_clk_lpuart8, pctl_mux_gpio_disp_b2_08, pctl_pad_gpio_disp_b2_08, pctl_mux_gpio_disp_b2_09, pctl_pad_gpio_disp_b2_09, pctl_isel_lpuart8_txd, 1, pctl_isel_lpuart8_rxd, 1 },
		{ (void *)0x4009c000, 3, pctl_clk_lpuart9, pctl_mux_gpio_sd_b2_00, pctl_pad_gpio_sd_b2_00, pctl_mux_gpio_sd_b2_01, pctl_pad_gpio_sd_b2_01, -1, -1, -1, -1 },
		{ (void *)0x400a0000, 1, pctl_clk_lpuart10, pctl_mux_gpio_ad_15, pctl_pad_gpio_ad_15, pctl_mux_gpio_ad_16, pctl_pad_gpio_ad_16, pctl_isel_lpuart10_txd, 0, pctl_isel_lpuart10_rxd, 0 },
		{ (void *)0x40c24000, 0, pctl_clk_lpuart11, pctl_mux_gpio_lpsr_08, pctl_pad_gpio_lpsr_08, pctl_mux_gpio_lpsr_09, pctl_pad_gpio_lpsr_09, pctl_isel_lpuart11_txd, 1, pctl_isel_lpuart11_rxd, 1 },
		{ (void *)0x40c28000, 6, pctl_clk_lpuart12, pctl_mux_gpio_lpsr_00, pctl_pad_gpio_lpsr_00, pctl_mux_gpio_lpsr_01, pctl_pad_gpio_lpsr_01, pctl_isel_lpuart12_txd, 0, pctl_isel_lpuart12_rxd, 0 }
	};

	console_common.uart = info[console].base;

	_imxrt_setDevClock(info[console].clk, 0, 0, 0, 0, 1);

	/* tx */
	_imxrt_setIOmux(info[console].txmux, 0, info[console].mode);
	_imxrt_setIOpad(info[console].txpad, 0, 0, 0, 0, 0, 0);

	if (info[console].txdaisy >= 0)
		_imxrt_setIOisel(info[console].txdaisy, info[console].txsel);

	/* rx */
	_imxrt_setIOmux(info[console].rxmux, 0, info[console].mode);
	_imxrt_setIOpad(info[console].rxpad, 0, 0, 1, 1, 0, 0);

	if (info[console].rxdaisy >= 0)
		_imxrt_setIOisel(info[console].rxdaisy, info[console].rxsel);

	/* Reset all internal logic and registers, except the Global Register */
	*(console_common.uart + uart_global) |= 1 << 1;
	hal_cpuDataMemoryBarrier();
	*(console_common.uart + uart_global) &= ~(1 << 1);
	hal_cpuDataMemoryBarrier();

	/* Set baud rate */
	t = *(console_common.uart + uart_baud) & ~((0x1f << 24) | (1 << 17) | 0xfff);

	/* For baud rate calculation, default UART_CLK=24MHz assumed */
	switch (CONSOLE_BAUDRATE) {
		case 9600: t |= 0x03020271; break;
		case 19200: t |= 0x03020138; break;
		case 38400: t |= 0x0302009c; break;
		case 57600: t |= 0x03020068; break;
		case 115200: t |= 0x03020034; break;
		case 230400: t |= 0x0302001a; break;
		case 460800: t |= 0x0302000d; break;
		/* As fallback use default 115200 */
		default: t |= 0x03020034; break;
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
