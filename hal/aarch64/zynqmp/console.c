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
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "hal/console.h"
#include "hal/cpu.h"

#include "hal/aarch64/dtb.h"
#include "hal/aarch64/arch/pmap.h"
#include "zynqmp.h"

#include <board_config.h>

#ifdef ZYNQMP_VIRT
/* Fallback QEMU virt UART0 base address */
#define QEMU_VIRT_UART0_BASE 0x09000000
#define PL011_FR_TXFF (1 << 5)
#define PL011_FR_BUSY (1 << 3)
#else
#if UART_CONSOLE_KERNEL == 0
#define UART_RX    UART0_RX
#define UART_TX    UART0_TX
#define UART_RESET pctl_devreset_lpd_uart0
#else
#define UART_RX    UART1_RX
#define UART_TX    UART1_TX
#define UART_RESET pctl_devreset_lpd_uart1
#endif
#endif


struct {
	volatile u32 *uart;
	u32 speed;
} console_common;


#ifdef ZYNQMP_VIRT
enum {
	dr = 0x00,
	fr = 0x06,
	ibrd = 0x09,
	fbrd = 0x0a,
	lcr_h = 0x0b,
	cr = 0x0c,
	imsc = 0x0e,
	icr = 0x11,
};
#else
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
#endif


static void _hal_consolePrint(const char *s)
{
	for (; *s != '\0'; s++) {
		hal_consolePutch(*s);
	}

#ifdef ZYNQMP_VIRT
	while ((*(console_common.uart + fr) & PL011_FR_BUSY) != 0) {
		/* Wait until the UART is no longer busy transmitting */
	}
#else
	while ((*(console_common.uart + sr) & (1U << 3)) == 0U) {
		/* Wait until TX fifo is empty */
	}
#endif
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
#ifdef ZYNQMP_VIRT
	while ((*(console_common.uart + fr) & PL011_FR_TXFF) != 0) {
		/* Wait until the PL011 Transmit FIFO is NOT full */
	}
	*(console_common.uart + dr) = c;
#else
	while ((*(console_common.uart + sr) & (1U << 3)) == 0U) {
		/* Wait until TX fifo is empty */
	}
	*(console_common.uart + fifo) = (u32)c;
#endif

}

__attribute__((section(".init"))) void _hal_consoleInit(void)
{
	dtb_serial_t *serials;
	size_t nSerials = 0;
	addr_t uart_base;
#ifdef ZYNQMP_VIRT
	uart_base = QEMU_VIRT_UART0_BASE;
#endif
	dtb_getSerials(&serials, &nSerials);
	if (UART_CONSOLE_KERNEL < nSerials) {
		uart_base = serials[UART_CONSOLE_KERNEL].base;
	}
#ifndef ZYNQMP_VIRT
	else {
		return;
	}
#endif

	console_common.uart = _pmap_halMapDevice(uart_base, 0, SIZE_PAGE);
	console_common.speed = 115200;

#ifdef ZYNQMP_VIRT
	/* Disable UART entirely before changing configuration */
	*(console_common.uart + cr) = 0x0;

	/* Clear any pending interrupts */
	*(console_common.uart + icr) = 0x7ff;

	/* Disable all interrupts for early console polling mode */
	//*(console_common.uart + imsc) = 0x0;

	/* Set parameters on Line Control High Register:
	 * WLEN = 8 bits (0x60), Enable FIFOs (0x10) -> 0x70 */
	*(console_common.uart + lcr_h) = 0x70;

	/* Re-enable UART, Transmit Enable (TXE), and Receive Enable (RXE)
	 * CR Bit 0 = UARTEN, Bit 8 = TXE, Bit 9 = RXE -> 0x301 */
	*(console_common.uart + cr) = (1 << 0) | (1 << 8) | (1 << 9);
#else
#if (UART_CONSOLE_ROUTED_VIA_PL != 1)
	(void)_zynqmp_setMIO(UART_TX, 0U, 0U, 0U, 6U, PCTL_MIO_SLOW_nFAST | PCTL_MIO_PULL_UP_nDOWN | PCTL_MIO_PULL_ENABLE);
	(void)_zynqmp_setMIO(UART_RX, 0U, 0U, 0U, 6U, PCTL_MIO_SLOW_nFAST | PCTL_MIO_PULL_UP_nDOWN | PCTL_MIO_PULL_ENABLE | PCTL_MIO_TRI_ENABLE);
#endif
	(void)_zynq_setDevRst(UART_RESET, 0U);

	*(console_common.uart + idr) = 0xfff;

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
#endif
}
