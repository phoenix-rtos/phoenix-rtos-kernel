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


#include "hal/console.h"
#include "hal/cpu.h"

#include "zynq.h"

#include <board_config.h>


#if UART_CONSOLE_KERNEL == 0U
#define UART     uart0
#define UART_RX  UART0_RX
#define UART_TX  UART0_TX
#define UART_CLK 20
#else
#define UART     uart1
#define UART_RX  UART1_RX
#define UART_TX  UART1_TX
#define UART_CLK 21
#endif


static struct {
	volatile u32 *uart0;
	volatile u32 *uart1;
	u8 type;
	u32 speed;
} console_common;


enum {
	cr = 0,
	mr,
	ier,
	idr,
	imr,
	isr,
	baudgen,
	rxtout,
	rxwm,
	modemcr,
	modemsr,
	sr,
	fifo,
	baud_rate_divider_reg0,
	flow_delay_reg0,
	tx_fifo_trigger_level0,
};


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _end;


static void _hal_consolePrint(const char *s)
{
	for (; *s != '\0'; s++) {
		hal_consolePutch(*s);
	}

	/* Wait until TX fifo is empty */
	while ((*(console_common.UART + sr) & (1U << 3)) == 0U) {
		;
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
	/* Wait until TX fifo is empty */
	while ((*(console_common.UART + sr) & (1U << 3)) == 0U) {
		;
	}

	*(console_common.UART + fifo) = (u32)c;
}


__attribute__((section(".init"))) void _hal_consoleInit(void)
{
	console_common.uart0 = (void *)(((u32)&_end + 3U * SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U));
	console_common.uart1 = (void *)(((u32)&_end + 4U * SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U));
	console_common.speed = 115200;

	_zynq_setMIO(UART_RX, 1, 1, 1, 0, 0, 0, 0, 0x7, 1);
	_zynq_setMIO(UART_TX, 1, 1, 1, 0, 0, 0, 0, 0x7, 0);

	(void)_zynq_setAmbaClk(UART_CLK, 1);

	*(console_common.UART + idr) = 0xfff;

	/* Uart Mode Register
	 * normal mode, 1 stop bit, no parity, 8 bits, uart_ref_clk as source clock
	 * PAR = 0x4 */
	*(console_common.UART + mr) = (*(console_common.UART + mr) & ~0x000003ffU) | 0x00000020U;

	/* Disable TX and RX */
	*(console_common.UART + cr) = (*(console_common.UART + cr) & ~0x000001ffU) | 0x00000028U;

	/* Assumptions:
	 * - baudarate : 115200
	 * - ref_clk   : 50 MHz
	 * - baud_rate = ref_clk / (bgen * (bdiv + 1)) */
	*(console_common.UART + baudgen) = 62;
	*(console_common.UART + baud_rate_divider_reg0) = 6;

	/* Uart Control Register
	 * TXEN = 0x1; RXEN = 0x1; TXRES = 0x1; RXRES = 0x1 */
	*(console_common.UART + cr) = (*(console_common.UART + cr) & ~0x000001ffU) | 0x00000017U;
}
