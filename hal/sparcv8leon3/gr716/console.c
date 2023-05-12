/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "gr716.h"
#include "../sparcv8leon3.h"
#include "../../console.h"
#include "../../cpu.h"
#include "../../../include/arch/gr716.h"

#include <board_config.h>


/* UART control bits */
#define TX_EN (1 << 1)

/* UART status bits */
#define TX_FIFO_FULL (1 << 9)

/* Console config */
#define CONSOLE_RX       UART2_RX
#define CONSOLE_TX       UART2_TX
#define CONSOLE_BASE     UART2_BASE
#define CONSOLE_CGU      cgudev_apbuart2
#define CONSOLE_BAUDRATE UART_BAUDRATE


enum {
	uart_data,   /* Data register           : 0x00 */
	uart_status, /* Status register         : 0x04 */
	uart_ctrl,   /* Control register        : 0x08 */
	uart_scaler, /* Scaler reload register  : 0x0C */
	uart_dbg     /* FIFO debug register     : 0x10 */
};


struct {
	volatile u32 *uart;
} halconsole_common;


static void _hal_consolePrint(const char *s)
{
	for (; *s; s++) {
		hal_consolePutch(*s);
	}

	/* Wait until TX fifo is empty */
	while ((*(halconsole_common.uart + uart_status) & TX_FIFO_FULL) != 0) {
	}
}


static int _hal_consoleSetPin(u8 pin)
{
	int err = 0;
	u8 dir;
	u8 opt = 0x1, pullup = 0, pulldn = 0;

	switch (pin) {
		case CONSOLE_TX:
			dir = GPIO_DIR_OUT;
			break;
		case CONSOLE_RX:
			dir = GPIO_DIR_IN;
			break;
		default:
			err = -1;
			break;
	}

	return err == 0 ? _gr716_setIOCfg(pin, opt, dir, pullup, pulldn) : err;
}


static u32 _hal_consoleCalcScaler(u32 baud)
{
	u32 scaler = 0;

	scaler = (SYSCLK_FREQ / (baud * 8 + 7));

	return scaler;
}


void hal_consolePutch(char c)
{
	/* Wait until TX fifo is empty */
	while ((*(halconsole_common.uart + uart_status) & TX_FIFO_FULL) != 0) {
	}
	*(halconsole_common.uart + uart_data) = c;
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


void _hal_consoleInit(void)
{
	_hal_consoleSetPin(CONSOLE_TX);
	_hal_consoleSetPin(CONSOLE_RX);
	_gr716_cguClkEnable(cgu_primary, CONSOLE_CGU);
	halconsole_common.uart = CONSOLE_BASE;
	*(halconsole_common.uart + uart_ctrl) = TX_EN;
	*(halconsole_common.uart + uart_scaler) = _hal_consoleCalcScaler(CONSOLE_BAUDRATE);
	hal_cpuDataStoreBarrier();
}
