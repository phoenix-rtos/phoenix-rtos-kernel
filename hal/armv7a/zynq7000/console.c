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
#include "hal/pmap.h"

#include "zynq.h"

#include <board_config.h>


#if UART_CONSOLE_KERNEL == 0
#define UART_BASE 0xe0000000U
#define UART_RX  UART0_RX
#define UART_TX  UART0_TX
#define UART_CLK 20
#else
#define UART_BASE 0xe0001000U
#define UART_RX  UART1_RX
#define UART_TX  UART1_TX
#define UART_CLK 21
#endif


static struct {
	volatile u32 *base;
	u8 type;
	u32 speed;
} console_common;


/* clang-format off */
enum {
	cr = 0, mr, ier, idr, imr, isr, baudgen, rxtout, rxwm, modemcr, modemsr, sr, fifo,
	baud_rate_divider_reg0, flow_delay_reg0, tx_fifo_trigger_level0,
};
/* clang-format on */


static void _hal_consolePrint(const char *s)
{
	for (; *s != '\0'; s++) {
		hal_consolePutch(*s);
	}

	/* Wait until TX fifo is empty */
	while ((*(console_common.base + sr) & (1U << 3)) == 0U) {
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
	while ((*(console_common.base + sr) & (1U << 3)) == 0U) {
		;
	}

	*(console_common.base + fifo) = (u32)c;
}


__attribute__((section(".init"))) void _hal_consoleInit(void)
{
	console_common.base = _pmap_halMapDevice(UART_BASE, 0, SIZE_PAGE);
	console_common.speed = 115200;

	(void)_zynq_setMIO(UART_RX, 1U, 1U, 1U, 0U, 0U, 0U, 0U, 0x7U, 1U);
	(void)_zynq_setMIO(UART_TX, 1U, 1U, 1U, 0U, 0U, 0U, 0U, 0x7U, 0U);

	(void)_zynq_setAmbaClk((u32)UART_CLK, 1U);

	*(console_common.base + idr) = 0xfffU;

	/* Uart Mode Register
	 * normal mode, 1 stop bit, no parity, 8 bits, uart_ref_clk as source clock
	 * PAR = 0x4 */
	*(console_common.base + mr) = (*(console_common.base + mr) & ~0x000003ffU) | 0x00000020U;

	/* Disable TX and RX */
	*(console_common.base + cr) = (*(console_common.base + cr) & ~0x000001ffU) | 0x00000028U;

	/* Assumptions:
	 * - baudarate : 115200
	 * - ref_clk   : 50 MHz
	 * - baud_rate = ref_clk / (bgen * (bdiv + 1)) */
	*(console_common.base + baudgen) = 62;
	*(console_common.base + baud_rate_divider_reg0) = 6;

	/* Uart Control Register
	 * TXEN = 0x1; RXEN = 0x1; TXRES = 0x1; RXRES = 0x1 */
	*(console_common.base + cr) = (*(console_common.base + cr) & ~0x000001ffU) | 0x00000017U;
}
