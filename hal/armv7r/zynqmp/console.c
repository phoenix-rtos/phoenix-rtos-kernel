/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (ANSI TTY via ZynqMP UART)
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

#include "zynqmp.h"

#include <board_config.h>


#if UART_CONSOLE_KERNEL == 0
#define UART_RX    UART0_RX
#define UART_TX    UART0_TX
#define UART_RESET pctl_devreset_lpd_uart0
#define UART_BASE  0xff000000U
#else
#define UART_RX    UART1_RX
#define UART_TX    UART1_TX
#define UART_RESET pctl_devreset_lpd_uart1
#define UART_BASE  0xff010000U
#endif


struct {
	volatile u32 *uart;
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


static void _hal_consolePrint(const char *s)
{
	for (; *s; s++) {
		hal_consolePutch(*s);
	}

	while ((*(console_common.uart + sr) & (0x1U << 3)) == 0U) {
		/* Wait until TX fifo is empty */
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

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void hal_consolePutch(char c)
{
	while ((*(console_common.uart + sr) & (0x1U << 3)) == 0U) {
		/* Wait until TX fifo is empty */
	}

	*(console_common.uart + fifo) = c;
}


__attribute__((section(".init"))) void _hal_consoleInit(void)
{
	console_common.uart = (void *)UART_BASE;
	console_common.speed = 115200;

	_zynqmp_setMIO(UART_TX, 0U, 0U, 0U, 6U, PCTL_MIO_SLOW_nFAST | PCTL_MIO_PULL_UP_nDOWN | PCTL_MIO_PULL_ENABLE);
	_zynqmp_setMIO(UART_RX, 0U, 0U, 0U, 6U, PCTL_MIO_SLOW_nFAST | PCTL_MIO_PULL_UP_nDOWN | PCTL_MIO_PULL_ENABLE | PCTL_MIO_TRI_ENABLE);

	_zynq_setDevRst(UART_RESET, 0);

	*(console_common.uart + idr) = 0xfffU;

	/* Uart Mode Register
	 * normal mode, 1 stop bit, no parity, 8 bits, uart_ref_clk as source clock
	 * PAR = 0x4 */
	*(console_common.uart + mr) = (*(console_common.uart + mr) & ~0x000003ffU) | 0x00000020U;

	/* Disable TX and RX */
	*(console_common.uart + cr) = (*(console_common.uart + cr) & ~0x000001ffU) | 0x00000028U;

	/* Assumptions:
	 * - baudrate : 115200
	 * - ref_clk   : 50 MHz
	 * - baud_rate = ref_clk / (bgen * (bdiv + 1)) */
	*(console_common.uart + baudgen) = 62;
	*(console_common.uart + baud_rate_divider_reg0) = 6;

	/* Uart Control Register
	 * TXEN = 0x1; RXEN = 0x1; TXRES = 0x1; RXRES = 0x1 */
	*(console_common.uart + cr) = (*(console_common.uart + cr) & ~0x000001ffU) | 0x00000017U;
}
