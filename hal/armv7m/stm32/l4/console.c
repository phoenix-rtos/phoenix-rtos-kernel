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

struct {
	volatile u32 *base;
	unsigned int cpufreq;
} console_common;


enum { cr1 = 0, cr2, cr3, brr, gtpr, rtor, rqr, isr, icr, rdr, tdr };


void _hal_consolePrint(const char *s)
{
	while (*s)
		hal_consolePutch(*(s++));

	while (~(*(console_common.base + isr)) & 0x80)
		;

	return;
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
	while (~(*(console_common.base + isr)) & 0x80)
		;

	*(console_common.base + tdr) = c;
}


void _hal_consoleInit(void)
{
	struct {
		void *base;
		u8 uart;
	} uarts[] = {
		{ (void *)0x40013800, pctl_usart1 }, /* USART1 */
		{ (void *)0x40004400, pctl_usart2 }, /* USART2 */
		{ (void *)0x40004800, pctl_usart3 }, /* USART3 */
		{ (void *)0x40004c00, pctl_uart4 }, /* UART4 */
		{ (void *)0x40005000, pctl_uart5 }  /* UART5 */
	};

	const int uart = 1, port = pctl_gpiod, txpin = 5, rxpin = 6, af = 7;

	_stm32_rccSetDevClock(port, 1);

	console_common.base = uarts[uart].base;

	/* Init tx pin - output, push-pull, high speed, no pull-up */
	_stm32_gpioConfig(port, txpin, 2, af, 0, 2, 0);

	/* Init rxd pin - input, push-pull, high speed, no pull-up */
	_stm32_gpioConfig(port, rxpin, 2, af, 0, 2, 0);

	/* Enable uart clock */
	_stm32_rccSetDevClock(uarts[uart].uart, 1);

	console_common.cpufreq = _stm32_rccGetCPUClock();

	/* Set up UART to 9600,8,n,1 16-bit oversampling */
	*(console_common.base + cr1) &= ~1;   /* disable USART */
	hal_cpuDataMemoryBarrier();
	*(console_common.base + cr1) = 0xa;
	*(console_common.base + cr2) = 0;
	*(console_common.base + cr3) = 0;
	*(console_common.base + brr) = console_common.cpufreq / 115200; /* 115200 baud rate */
	hal_cpuDataMemoryBarrier();
	*(console_common.base + cr1) |= 1;
	hal_cpuDataMemoryBarrier();

	return;
}
