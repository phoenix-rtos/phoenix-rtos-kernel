/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (ANSI TTY via 8250 UART)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Michał Mirosław
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "console.h"
#include "cpu.h"
#include "syspage.h"


struct {
	u8 type;
	u8 speed;
	void *base;
} console_common;


enum {
	thr = 0, rbr = 0, dll = 0, ier = 1, dlh = 1, iir = 2, fcr = 2,
	lcr = 3, mcr = 4, lsr = 5, msr = 6, scr = 7
};


static inline void _console_uartWrite(unsigned int reg, u8 val)
{
	if (!console_common.type)
		hal_outb(console_common.base + reg, val);
	else
		*(u8 *)(console_common.base + reg) = val;
	return;
}


static inline u8 _console_uartRead(unsigned int reg)
{
	if (!console_common.type)
		return hal_inb(console_common.base + reg);

	return *(u8 *)(console_common.base + reg);
}


static void _console_print(const char *s)
{
	for (; *s != NULL; s++) {

		/* Wait for transmitter readiness */
		while (!(_console_uartRead(lsr) & 0x20));

		_console_uartWrite(thr, *s);
	}
}


void hal_consolePrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD) {
		_console_print("\033[1m");
		_console_print(s);
		_console_print("\033[0m");
	}
	else if (attr != ATTR_USER) {
		_console_print("\033[36m");
		_console_print(s);
		_console_print("\033[0m");
	}
	else
		_console_print(s);		
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{
	void *bases[] = {
		(void *)0x3f8, (void *)0x2f8, (void *)0x3e8, (void *)0x2e8,   /* regular PC COMs */
		(void *)0x9000f000u, (void *)0x9000b000u                      /* Galileo UARTs */
	};

	if (syspage->console > 5)
		return;

	console_common.base = bases[syspage->console];
	console_common.speed = 0;

	/* Mam physical memory when Galilo device is used */
	if (syspage->console < 4)
		console_common.type = 0;
	else {
		console_common.type = 1;
		/*
		*(top) = (*(top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1);
		pmap_enter(console_common.base[syspage->console], top);
		*/
	}

	/* 115200 8n1 */
	_console_uartWrite(lcr, 0x83);
	_console_uartWrite(dlh, 0x01);
	_console_uartWrite(dll, 0x20);
	_console_uartWrite(lcr, 0x03);

	/* disable IRQ */
	_console_uartWrite(ier, 0x00);

	/* set DTR and RTS */
	_console_uartWrite(mcr, 0x03);

	/* enable FIFO */
	_console_uartWrite(fcr, 0x21);

	return;
}
