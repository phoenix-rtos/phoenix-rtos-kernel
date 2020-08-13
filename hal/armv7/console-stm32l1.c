/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (STM32L1 USART)
 *
 * Copyright 2016-2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "console.h"
#include "stm32.h"
#include "../../include/errno.h"


struct {
	volatile u32 *base;
	u8 txpin;
	u8 rxpin;
	unsigned cpufreq;
} console_common;


enum { sr = 0, dr, brr, cr1, cr2, cr3, gtpr };


void _hal_consolePrint(const char *s)
{
	unsigned cpufreq;

	while (*s) {
		if (~(*(console_common.base + sr)) & 0x80)
			continue;

		cpufreq = _stm32_rccGetCPUClock();

		if (cpufreq != console_common.cpufreq) {
			console_common.cpufreq = cpufreq;

			*(console_common.base + cr1) &= ~(1 << 13);
			*(console_common.base + brr) = cpufreq / 9600;
			*(console_common.base + cr1) |= 1 << 13;
		}

		*(console_common.base + dr) = *(s++);
	}

	while (~(*(console_common.base + sr)) & 0x80);

	return;
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
	u8 port, txpin, rxpin, af = 7;

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

	const int uart = 3;
	port = pctl_gpioc;
	txpin = 10;
	rxpin = 11;
	af = 8;

	_stm32_rccSetDevClock(port, 1);

	console_common.base = uarts[uart].base;
	console_common.txpin = txpin;
	console_common.rxpin = rxpin;

	/* Init tx pin - output, push-pull, high speed, no pull-up */
	_stm32_gpioConfig(port, console_common.txpin, 2, af, 0, 2, 0);

	/* Init rxd pin - input, push-pull, high speed, no pull-up */
	_stm32_gpioConfig(port, console_common.rxpin, 2, af, 0, 2, 0);

	/* Enable uart clock */
	_stm32_rccSetDevClock(uarts[uart].uart, 1);

	console_common.cpufreq = _stm32_rccGetCPUClock();

	/* Set up UART to 9600,8,n,1 16-bit oversampling */
	*(console_common.base + cr1) &= ~0x2000;   /* disable USART */
	*(console_common.base + cr2) = 0;          /* 1 start, 1 stop bit */
	*(console_common.base + cr1) = 0x8 | 0x4;  /* enable receiver, enable transmitter */
	*(console_common.base + cr3) = 0;          /* no aditional settings */
	*(console_common.base + brr) = console_common.cpufreq / 9600; /* 9600 baud rate */
	*(console_common.base + cr1) |= 0x2000;

	_hal_consolePrint("\033[2J");
	_hal_consolePrint("\033[f");

	return;
}
