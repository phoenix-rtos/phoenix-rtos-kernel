/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (nRF9160 UART)
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "nrf91.h"

#include "hal/console.h"
#include "hal/cpu.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include <board_config.h>

#define TX_DMA_SIZE 64


union {
	void *ptr;
	const char *str;
} u;


static struct {
	volatile u32 *base;
	u8 txPin;
	u8 rxPin;
	u8 rtsPin;
	u8 ctsPin;
	char txDma[TX_DMA_SIZE];
	int txDmaSize;
	spinlock_t busySp;
} console_common;


/* clang-format off */
enum { uarte_startrx = 0, uarte_stoprx, uarte_starttx, uarte_stoptx,
	uarte_events_cts = 64, uarte_events_txdrdy = 71, uarte_events_endtx, uarte_events_error, uarte_events_txstarted = 84,
	uarte_inten = 192, uarte_errorsrc = 288, uarte_intenset, uarte_intenclr, uarte_enable = 320,
	uarte_psel_rts = 322, uarte_psel_txd, uarte_psel_cts, uarte_psel_rxd, uarte_baudrate = 329,
	uarte_rxd_ptr = 333, uarte_rxd_maxcnt, uarte_rxd_amount, uarte_txd_ptr = 337, uarte_txd_maxcnt, uarte_txd_amount,
	uarte_config = 347 };
/* clang-format on */


/* Init pins according to nrf9160 product specification */
static void console_configPins(void)
{
	_nrf91_gpioConfig(console_common.txPin, gpio_output, gpio_nopull);
	_nrf91_gpioConfig(console_common.rxPin, gpio_input, gpio_nopull);
	_nrf91_gpioConfig(console_common.rtsPin, gpio_output, gpio_nopull);
	_nrf91_gpioConfig(console_common.ctsPin, gpio_input, gpio_pulldown);

	_nrf91_gpioSet(console_common.txPin, gpio_high);
	_nrf91_gpioSet(console_common.rtsPin, gpio_high);
}


/* Send cnt bytes of data pointed by ptr using dma in console uart instance */
static void console_dmaSend(void *ptr, size_t cnt)
{
	*(console_common.base + uarte_txd_ptr) = (u32)ptr;
	*(console_common.base + uarte_txd_maxcnt) = cnt;
	*(console_common.base + uarte_starttx) = 1U;
	while (*(console_common.base + uarte_events_txstarted) != 1U) {
	}
	*(console_common.base + uarte_events_txstarted) = 0U;

	while (*(console_common.base + uarte_events_endtx) != 1U) {
	}
	*(console_common.base + uarte_events_endtx) = 0U;
}


void _hal_consolePrint(const char *s)
{
	volatile char *tx_dma_buff = (volatile char *)console_common.txDma;
	int pos = 0;
	size_t len = 0;

	while (s[len] != '\0') {
		len++;
	}

	/* copy to RAM only if it's not there. (6.7.7. EasyDMA chapter in NRF9160 PS) */
	if (((u32)s & 0xe0000000U) == 0x20000000U) {
		/* avoid discarding const */
		u.str = s;
		console_dmaSend(u.ptr, len);
	}
	/* copy to RAM */
	else {
		while (s[pos] != '\0') {
			len = 0;
			while ((s[pos] != '\0') && (len < console_common.txDmaSize)) {
				tx_dma_buff[len++] = s[pos++];
			}
			console_dmaSend(console_common.txDma, len);
		}
	}
}


void hal_consolePrint(int attr, const char *s)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&console_common.busySp, &sc);

	if (attr == ATTR_BOLD) {
		_hal_consolePrint(CONSOLE_BOLD);
	}
	else if (attr != ATTR_USER) {
		_hal_consolePrint(CONSOLE_CYAN);
	}

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);

	hal_spinlockClear(&console_common.busySp, &sc);
}


void hal_consolePutch(const char c)
{
	volatile char *tx_dma_buff = (volatile char *)console_common.txDma;
	spinlock_ctx_t sc;

	hal_spinlockSet(&console_common.busySp, &sc);
	tx_dma_buff[0] = c;
	console_dmaSend(console_common.txDma, 1);
	hal_spinlockClear(&console_common.busySp, &sc);
}


/* UART0 supported for the hal console */
void _hal_consoleInit(void)
{
	const struct {
		u8 uart;
		volatile u32 *base;
		u8 txPin;
		u8 rxPin;
		u8 rtsPin;
		u8 ctsPin;
	} uarts[] = {
		{ 0, (u32 *)0x50008000U, UART0_TX, UART0_RX, UART0_RTS, UART0_CTS },
		{ 1, (u32 *)0x50009000U, UART1_TX, UART1_RX, UART1_RTS, UART1_CTS },
		{ 2, (u32 *)0x5000a000U, UART2_TX, UART2_RX, UART2_RTS, UART2_CTS },
		{ 3, (u32 *)0x5000b000U, UART3_TX, UART3_RX, UART3_RTS, UART3_CTS }
	};

	const int uart = UART_CONSOLE;
	console_common.base = uarts[uart].base;
	console_common.txPin = uarts[uart].txPin;
	console_common.rxPin = uarts[uart].rxPin;
	console_common.rtsPin = uarts[uart].rtsPin;
	console_common.ctsPin = uarts[uart].ctsPin;
	console_common.txDmaSize = sizeof(console_common.txDma);

	hal_spinlockCreate(&console_common.busySp, "dmaBusy");

	console_configPins();

	/* disable uarte instance */
	*(console_common.base + uarte_enable) = 0U;
	hal_cpuDataMemoryBarrier();

	/* Select pins */
	*(console_common.base + uarte_psel_txd) = console_common.txPin;
	*(console_common.base + uarte_psel_rxd) = console_common.rxPin;
	*(console_common.base + uarte_psel_rts) = console_common.rtsPin;
	*(console_common.base + uarte_psel_cts) = console_common.ctsPin;

	/* set baud rate to 115200 */
	*(console_common.base + uarte_baudrate) = 0x01d60000U;

	/* Default settings - hardware flow control disabled, exclude parity bit, one stop bit */
	*(console_common.base + uarte_config) = 0U;

	/* Set default max number of bytes in specific buffers */
	*(console_common.base + uarte_txd_maxcnt) = TX_DMA_SIZE;


	/* Set default memory regions for uart dma */
	*(console_common.base + uarte_txd_ptr) = (volatile u32)console_common.txDma;

	/* disable all uart interrupts */
	*(console_common.base + uarte_intenclr) = 0xffffffffU;
	hal_cpuDataMemoryBarrier();

	/* enable uarte instance */
	*(console_common.base + uarte_enable) = 0x8U;
	hal_cpuDataMemoryBarrier();
}
