/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (ANSI TTY via Zynq 7000 UART1)
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "../../console.h"
#include "../../cpu.h"


#define UART uart1


struct {
	volatile u32 *uart0;
	volatile u32 *uart1;
	u8 type;
	u32 speed;
} console_common;


enum {
	cr = 0, mr, ier, idr, imr, isr, baudgen, rxtout, rxwm, modemcr, modemsr, sr, fifo,
	baud_rate_divider_reg0, flow_delay_reg0, tx_fifo_trigger_level0,
};


extern unsigned int _end;


static void _hal_consolePrint(const char *s)
{
	for (; *s; s++)
		hal_consolePutch(*s);

	/* Wait until TX fifo is empty */
	while (!(*(console_common.UART + sr) & (1 << 3)))
		;
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
	/* Wait until TX fifo is empty */
	while (!(*(console_common.UART + sr) & (1 << 3)))
		;

	*(console_common.UART + fifo) = c;
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{
	console_common.uart0 = (void *)(((u32)&_end + 3 * SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	console_common.uart1 = (void *)(((u32)&_end + 4 * SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	console_common.speed = 115200;

	*(console_common.UART + idr) = 0xfff;

	/* Uart Mode Register
	* normal mode, 1 stop bit, no parity, 8 bits, uart_ref_clk as source clock
	* PAR = 0x4 */
	*(console_common.UART + mr) = (*(console_common.UART + mr) & ~0x000003ff) | 0x00000020;

	/* Disable TX and RX */
	*(console_common.UART + cr) = (*(console_common.UART + cr) & ~0x000001ff) | 0x00000028;

	/* Assumptions:
	 * - baudarate : 115200
	 * - ref_clk   : 50 MHz
	 * - baud_rate = ref_clk / (bgen * (bdiv + 1)) */
	*(console_common.UART + baudgen) = 62;
	*(console_common.UART + baud_rate_divider_reg0) = 6;

	/* Uart Control Register
	* TXEN = 0x1; RXEN = 0x1; TXRES = 0x1; RXRES = 0x1 */
	*(console_common.UART + cr) = (*(console_common.UART + cr) & ~0x000001ff) | 0x00000017;
}
