/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (STM32L4 USART)
 *
 * Copyright 2016-2017, 2019-2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/console.h"
#include "hal/cpu.h"

#include "stm32.h"

static struct {
	volatile u32 *base;
	unsigned int cpufreq;
} console_common;


/* clang-format off */
enum { cr1 = 0, cr2, cr3, brr, gtpr, rtor, rqr, isr, icr, rdr, tdr };
/* clang-format on */


static void _hal_consolePrint(const char *s)
{
	while (*s != '\0') {
		hal_consolePutch(*(s++));
	}

	while ((~(*(console_common.base + isr)) & 0x80U) != 0U) { }

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
	else {
		/* No action required */
	}

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void hal_consolePutch(char c)
{
	while ((~(*(console_common.base + isr)) & 0x80U) != 0U) { }

	*(console_common.base + tdr) = (u32)c;
}


void _hal_consoleInit(void)
{
	struct {
		void *base;
		int uart;
	} uarts[] = {
		{ (void *)0x40013800U, pctl_usart1 }, /* USART1 */
		{ (void *)0x40004400U, pctl_usart2 }, /* USART2 */
		{ (void *)0x40004800U, pctl_usart3 }, /* USART3 */
		{ (void *)0x40004c00U, pctl_uart4 },  /* UART4 */
		{ (void *)0x40005000U, pctl_uart5 }   /* UART5 */
	};

	const int port = pctl_gpiod;
	const u8 uart = 1U, txpin = 5U, rxpin = 6U, af = 7U;

	(void)_stm32_rccSetDevClock(port, 1U);

	console_common.base = uarts[uart].base;

	/* Init tx pin - output, push-pull, high speed, no pull-up */
	(void)_stm32_gpioConfig(port, txpin, 2U, af, 0U, 2U, 0U);

	/* Init rxd pin - input, push-pull, high speed, no pull-up */
	(void)_stm32_gpioConfig(port, rxpin, 2U, af, 0U, 2U, 0U);

	/* Enable uart clock */
	(void)_stm32_rccSetDevClock(uarts[uart].uart, 1U);

	console_common.cpufreq = _stm32_rccGetCPUClock();

	/* Set up UART to 9600,8,n,1 16-bit oversampling */
	*(console_common.base + cr1) &= ~1U; /* disable USART */
	hal_cpuDataMemoryBarrier();
	*(console_common.base + cr1) = 0xaU;
	*(console_common.base + cr2) = 0U;
	*(console_common.base + cr3) = 0U;
	*(console_common.base + brr) = console_common.cpufreq / 115200U; /* 115200 baud rate */
	hal_cpuDataMemoryBarrier();
	*(console_common.base + cr1) |= 1U;
	hal_cpuDataMemoryBarrier();

	return;
}
