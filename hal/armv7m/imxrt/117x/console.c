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
/* parasoft-suppress-next-line MISRAC2012-RULE_20_7 "Cannot enclose macro parameters in parentheses as CONCAT3 concatenates literal tokens." */
#define CONSOLE_BAUD(n) (CONCAT3(UART, n, _BAUDRATE))

#if !ISEMPTY(UART_CONSOLE_KERNEL) && CONSOLE_BAUD(UART_CONSOLE_KERNEL)
#define CONSOLE_BAUDRATE CONSOLE_BAUD(UART_CONSOLE_KERNEL)
#else
#define CONSOLE_BAUDRATE 115200
#endif

static struct {
	volatile u32 *uart;
} console_common;


/* clang-format off */
enum { uart_verid = 0, uart_param, uart_global, uart_pincfg, uart_baud, uart_stat, uart_ctrl,
	uart_data, uart_match, uart_modir, uart_fifo, uart_water };
/* clang-format on */


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
#if RTT_ENABLED && !ISEMPTY(RTT_CONSOLE_KERNEL)
	_hal_rttWrite(RTT_CONSOLE_KERNEL, &c, 1);
#endif

#if !ISEMPTY(UART_CONSOLE_KERNEL)
	while ((*(console_common.uart + uart_stat) & (1UL << 23)) == 0U) { }

	*(console_common.uart + uart_data) = (u32)c;
#endif
}


#if !ISEMPTY(UART_CONSOLE_KERNEL)
static void _hal_uartInit(void)
{
	u32 t, console = (u32)UART_CONSOLE_KERNEL - 1U;

	static const struct {
		volatile u32 *base;
		int mode;
		int clk;
		int txmux, txpad;
		int rxmux, rxpad;
		int txdaisy, txsel;
		int rxdaisy, rxsel;
	} info[] = {
		{ (void *)0x4007c000U, 0, pctl_clk_lpuart1, pctl_mux_gpio_ad_24, pctl_pad_gpio_ad_24, pctl_mux_gpio_ad_25, pctl_pad_gpio_ad_25, pctl_isel_lpuart1_txd, 0, pctl_isel_lpuart1_rxd, 0 },
		{ (void *)0x40080000U, 2, pctl_clk_lpuart2, pctl_mux_gpio_disp_b2_10, pctl_pad_gpio_disp_b2_10, pctl_mux_gpio_disp_b2_11, pctl_pad_gpio_disp_b2_11, -1, -1, -1, -1 },
		{ (void *)0x40084000U, 4, pctl_clk_lpuart3, pctl_mux_gpio_ad_30, pctl_pad_gpio_ad_30, pctl_mux_gpio_ad_31, pctl_pad_gpio_ad_31, -1, -1, -1, -1 },
		{ (void *)0x40088000U, 2, pctl_clk_lpuart4, pctl_mux_gpio_disp_b1_06, pctl_pad_gpio_disp_b1_06, pctl_mux_gpio_disp_b1_04, pctl_pad_gpio_disp_b1_04, -1, -1, -1, -1 },
		{ (void *)0x4008c000U, 1, pctl_clk_lpuart5, pctl_mux_gpio_ad_28, pctl_pad_gpio_ad_28, pctl_mux_gpio_ad_29, pctl_pad_gpio_ad_29, -1, -1, -1, -1 },
		{ (void *)0x40090000U, 3, pctl_clk_lpuart6, pctl_mux_gpio_emc_b1_40, pctl_pad_gpio_emc_b1_40, pctl_mux_gpio_emc_b1_41, pctl_pad_gpio_emc_b1_41, -1, -1, -1, -1 },
		{ (void *)0x40094000U, 2, pctl_clk_lpuart7, pctl_mux_gpio_disp_b2_06, pctl_pad_gpio_disp_b2_06, pctl_mux_gpio_disp_b2_07, pctl_pad_gpio_disp_b2_07, pctl_isel_lpuart7_txd, 1, pctl_isel_lpuart7_rxd, 1 },
		{ (void *)0x40098000U, 2, pctl_clk_lpuart8, pctl_mux_gpio_disp_b2_08, pctl_pad_gpio_disp_b2_08, pctl_mux_gpio_disp_b2_09, pctl_pad_gpio_disp_b2_09, pctl_isel_lpuart8_txd, 1, pctl_isel_lpuart8_rxd, 1 },
		{ (void *)0x4009c000U, 3, pctl_clk_lpuart9, pctl_mux_gpio_sd_b2_00, pctl_pad_gpio_sd_b2_00, pctl_mux_gpio_sd_b2_01, pctl_pad_gpio_sd_b2_01, -1, -1, -1, -1 },
		{ (void *)0x400a0000U, 1, pctl_clk_lpuart10, pctl_mux_gpio_ad_15, pctl_pad_gpio_ad_15, pctl_mux_gpio_ad_16, pctl_pad_gpio_ad_16, pctl_isel_lpuart10_txd, 0, pctl_isel_lpuart10_rxd, 0 },
		{ (void *)0x40c24000U, 0, pctl_clk_lpuart11, pctl_mux_gpio_lpsr_08, pctl_pad_gpio_lpsr_08, pctl_mux_gpio_lpsr_09, pctl_pad_gpio_lpsr_09, pctl_isel_lpuart11_txd, 1, pctl_isel_lpuart11_rxd, 1 },
		{ (void *)0x40c28000U, 6, pctl_clk_lpuart12, pctl_mux_gpio_lpsr_00, pctl_pad_gpio_lpsr_00, pctl_mux_gpio_lpsr_01, pctl_pad_gpio_lpsr_01, pctl_isel_lpuart12_txd, 0, pctl_isel_lpuart12_rxd, 0 }
	};

	console_common.uart = info[console].base;

	(void)_imxrt_setDevClock(info[console].clk, 0, 0, 0, 0, 1);

	/* tx */
	(void)_imxrt_setIOmux(info[console].txmux, 0, info[console].mode);
	(void)_imxrt_setIOpad(info[console].txpad, 0U, 0U, 0U, 0U, 0U, 0U);

	if (info[console].txdaisy >= 0) {
		(void)_imxrt_setIOisel(info[console].txdaisy, info[console].txsel);
	}

	/* rx */
	(void)_imxrt_setIOmux(info[console].rxmux, 0, info[console].mode);
	(void)_imxrt_setIOpad(info[console].rxpad, 0U, 0U, 1U, 1U, 0U, 0U);

	if (info[console].rxdaisy >= 0) {
		(void)_imxrt_setIOisel(info[console].rxdaisy, info[console].rxsel);
	}

	/* Reset all internal logic and registers, except the Global Register */
	*(console_common.uart + uart_global) |= 1U << 1;
	hal_cpuDataMemoryBarrier();
	*(console_common.uart + uart_global) &= ~(1U << 1);
	hal_cpuDataMemoryBarrier();

	/* Set baud rate */
	t = *(console_common.uart + uart_baud) & ~((0x1fUL << 24) | (1UL << 17) | 0xfffU);

	/* For baud rate calculation, default UART_CLK=24MHz assumed */
#if CONSOLE_BAUDRATE == 9600
	t |= 0x03020271U;
#elif CONSOLE_BAUDRATE == 19200
	t |= 0x03020138U;
#elif CONSOLE_BAUDRATE == 38400
	t |= 0x0302009cU;
#elif CONSOLE_BAUDRATE == 57600
	t |= 0x03020068U;
#elif CONSOLE_BAUDRATE == 115200
	t |= 0x03020034U;
#elif CONSOLE_BAUDRATE == 230400
	t |= 0x0302001aU;
#elif CONSOLE_BAUDRATE == 460800
	t |= 0x0302000dU;
#else
	/* As fallback use default 115200 */
	t |= 0x03020034U;
#endif

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
#endif


void _hal_consoleInit(void)
{
#if RTT_ENABLED && !ISEMPTY(RTT_CONSOLE_KERNEL)
	_hal_rttSetup();
#endif

#if !ISEMPTY(UART_CONSOLE_KERNEL)
	_hal_uartInit();
#endif
}
