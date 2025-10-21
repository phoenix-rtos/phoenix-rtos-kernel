/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (STM32N6 USART)
 *
 * Copyright 2016-2017, 2019-2020, 2025 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/console.h"
#include "hal/cpu.h"

#include <board_config.h>

#include "hal/armv8m/stm32/stm32.h"

#ifndef UART_CONSOLE_KERNEL
#define UART_CONSOLE_KERNEL UART_CONSOLE
#endif

#if (UART_CONSOLE_KERNEL <= 0U)
#error "UART_CONSOLE_KERNEL set incorrectly"
#endif

#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)

#define UART_IO_PORT_DEV CONCAT(pctl_, UART_IO_PORT)

#define UART_ISR_TXE (1 << 7)


static struct {
	volatile u32 *base;
	unsigned int refclkfreq;
} console_common;


/* Values for selecting the peripheral clock for an UART */
enum {
	uart_clk_sel_pclk = 0, /* pclk1 or pclk2 depending on peripheral */
	uart_clk_sel_per_ck,
	uart_clk_sel_ic9_ck,
	uart_clk_sel_ic14_ck,
	uart_clk_sel_lse_ck,
	uart_clk_sel_msi_ck,
	uart_clk_sel_hsi_div_ck,
};


/* clang-format off */
enum { cr1 = 0, cr2, cr3, brr, gtpr, rtor, rqr, isr, icr, rdr, tdr, presc };
/* clang-format on */


void _hal_consolePrint(const char *s)
{
	while (*s) {
		hal_consolePutch(*(s++));
	}

	while (((*(console_common.base + isr)) & UART_ISR_TXE) == 0) {
		/* Wait for transmit register empty */
	}

	return;
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


void hal_consolePutch(char c)
{
	while (((*(console_common.base + isr)) & UART_ISR_TXE) == 0) {
		/* Wait for transmit register empty */
	}

	*(console_common.base + tdr) = c;
}


void _hal_consoleInit(void)
{
	static const struct {
		void *base;
		u16 dev_clk;
		u8 ipclk_sel;
	} uarts[] = {
		{ ((void *)0x52001000), pctl_usart1, pctl_ipclk_usart1sel },
		{ ((void *)0x50004400), pctl_usart2, pctl_ipclk_usart2sel },
		{ ((void *)0x50004800), pctl_usart3, pctl_ipclk_usart3sel },
		{ ((void *)0x50004c00), pctl_uart4, pctl_ipclk_uart4sel },
		{ ((void *)0x50005000), pctl_uart5, pctl_ipclk_uart5sel },
		{ ((void *)0x52001400), pctl_usart6, pctl_ipclk_usart6sel },
		{ ((void *)0x50007800), pctl_uart7, pctl_ipclk_uart7sel },
		{ ((void *)0x50007c00), pctl_uart8, pctl_ipclk_uart8sel },
		{ ((void *)0x52001800), pctl_uart9, pctl_ipclk_uart9sel },
		{ ((void *)0x52001c00), pctl_usart10, pctl_ipclk_usart10sel },
	};

	const int uart = UART_CONSOLE_KERNEL - 1U, port = UART_IO_PORT_DEV, txpin = UART_PIN_TX, rxpin = UART_PIN_RX, af = UART_IO_AF;

	_stm32_rccSetDevClock(port, 1, 1);

	console_common.base = uarts[uart].base;

	/* Init tx pin - output, push-pull, low speed, no pull-up */
	_stm32_gpioConfig(port, txpin, gpio_mode_af, af, gpio_otype_pp, gpio_ospeed_low, gpio_pupd_nopull);

	/* Init rxd pin - input, push-pull, low speed, no pull-up */
	_stm32_gpioConfig(port, rxpin, gpio_mode_af, af, gpio_otype_pp, gpio_ospeed_low, gpio_pupd_nopull);

	_stm32_rccSetDevClock(pctl_per, 1, 1);
	_stm32_rccSetIPClk(uarts[uart].ipclk_sel, uart_clk_sel_per_ck);
	console_common.refclkfreq = _stm32_rccGetPerClock();

	/* Enable uart clock */
	_stm32_rccSetDevClock(uarts[uart].dev_clk, 1, 1);

	/* Set up UART to 115200,8,n,1 16-bit oversampling */
	*(console_common.base + cr1) &= ~1; /* disable USART */
	hal_cpuDataMemoryBarrier();
	*(console_common.base + cr1) = 0xe; /* Enable TX and RX, UART enabled in low-power mode */
	*(console_common.base + cr2) = 0;
	*(console_common.base + cr3) = 0;
	*(console_common.base + brr) = console_common.refclkfreq / 115200; /* 115200 baud rate */
	hal_cpuDataMemoryBarrier();
	*(console_common.base + cr1) |= 1;
	hal_cpuDataMemoryBarrier();
}
