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

#include "hal/console.h"
#include "hal/ia32/console.h"
#include "ia32.h"
#include "halsyspage.h"


static struct {
	u8 type;
	u8 speed;
	void *base;
} console_common;


/* clang-format off */
enum {
	thr = 0, rbr = 0, dll = 0, ier = 1, dlh = 1, iir = 2, fcr = 2,
	lcr = 3, mcr = 4, lsr = 5, msr = 6, scr = 7
};
/* clang-format on */


static inline void _console_uartWrite(unsigned int reg, u8 val)
{
	if (console_common.type == 0) {
		hal_outb((u16)((addr_t)console_common.base + reg), val);
	}
	else {
		*(u8 *)(console_common.base + reg) = val;
	}
	return;
}


static inline u8 _console_uartRead(unsigned int reg)
{
	if (console_common.type == 0) {
		return hal_inb((u16)((addr_t)console_common.base + reg));
	}
	return *(u8 *)(console_common.base + reg);
}


void hal_consoleSerialPutch(char c)
{
	/* Wait for transmitter readiness */
	while (!(_console_uartRead(lsr) & 0x20))
		;

	_console_uartWrite(thr, c);
}


static void _hal_consolePrint(const char *s)
{
	for (; *s; s++)
		hal_consoleSerialPutch(*s);
}


void hal_consoleSerialPrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD)
		_hal_consolePrint(CONSOLE_BOLD);
	else if (attr != ATTR_USER)
		_hal_consolePrint(CONSOLE_CYAN);

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


__attribute__((section(".init"))) void _hal_consoleSerialInit(void)
{
	void *bases[] = {
		(void *)0x3f8, (void *)0x2f8, (void *)0x3e8, (void *)0x2e8, /* regular PC COMs */
		(void *)0x9000f000u, (void *)0x9000b000u                    /* Galileo UARTs */
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
	_console_uartWrite(lcr, 0x80);
	_console_uartWrite(dll, 0x01);
	_console_uartWrite(dlh, 0x00);
	_console_uartWrite(lcr, 0x03);

	/* disable IRQ */
	_console_uartWrite(ier, 0x00);

	/* set DTR and RTS */
	_console_uartWrite(mcr, 0x03);

	/* enable FIFO */
	_console_uartWrite(fcr, 0x21);

	return;
}
