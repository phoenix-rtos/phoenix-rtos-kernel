/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console
 *
 * Copyright 2022-2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/sparcv8leon3/sparcv8leon3.h"
#include "hal/console.h"
#include "hal/cpu.h"
#include "gaisler.h"

#include <board_config.h>


extern unsigned int _end;


#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)


/* UART control bits */
#define TX_EN (1 << 1)

/* UART status bits */
#define TX_FIFO_FULL (1 << 9)

/* Console config */
#define CONSOLE_RX       CONCAT(UART, CONCAT(UART_CONSOLE_KERNEL, _RX))
#define CONSOLE_TX       CONCAT(UART, CONCAT(UART_CONSOLE_KERNEL, _TX))
#define CONSOLE_CGU      CONCAT(cgudev_apbuart, UART_CONSOLE_KERNEL)
#define CONSOLE_BAUDRATE UART_BAUDRATE

#ifdef NOMMU
#define VADDR_CONSOLE CONCAT(UART, CONCAT(UART_CONSOLE_KERNEL, _BASE))
#else
#define VADDR_CONSOLE (void *)((u32)VADDR_PERIPH_BASE + PAGE_OFFS_CONSOLE)
#endif


enum {
	uart_data = 0, /* Data register           : 0x00 */
	uart_status,   /* Status register         : 0x04 */
	uart_ctrl,     /* Control register        : 0x08 */
	uart_scaler,   /* Scaler reload register  : 0x0C */
	uart_dbg       /* FIFO debug register     : 0x10 */
};


static struct {
	volatile u32 *uart;
} halconsole_common;


/* CPU-specific functions */

#if defined(__CPU_GR716)

static void console_cguClkEnable(void)
{
	_gr716_cguClkEnable(cgu_primary, CONSOLE_CGU);
}


static int console_cguClkStatus(void)
{
	return _gr716_cguClkStatus(cgu_primary, CONSOLE_CGU);
}


static void console_iomuxCfg(void)
{
	gaisler_setIomuxCfg(CONSOLE_TX, 0x1, 0, 0);
	gaisler_setIomuxCfg(CONSOLE_RX, 0x1, 0, 0);
}

#else

static void console_cguClkEnable(void)
{
}


static int console_cguClkStatus(void)
{
	return 1;
}


static void console_iomuxCfg(void)
{
}

#endif


static void _hal_consolePrint(const char *s)
{
	for (; *s; s++) {
		hal_consolePutch(*s);
	}

	/* Wait until TX fifo is empty */
	while ((*(halconsole_common.uart + uart_status) & TX_FIFO_FULL) != 0) {
	}
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
	halconsole_common.uart = VADDR_CONSOLE;

	*(halconsole_common.uart + uart_ctrl) = 0;

	console_iomuxCfg();

	if (console_cguClkStatus() == 0) {
		console_cguClkEnable();
	}

	/* Clear UART FIFO */
	while ((*(halconsole_common.uart + uart_status) & (1 << 0)) != 0) {
		(void)*(halconsole_common.uart + uart_data);
	}
	*(halconsole_common.uart + uart_scaler) = _hal_consoleCalcScaler(CONSOLE_BAUDRATE);
	*(halconsole_common.uart + uart_ctrl) = TX_EN;
	hal_cpuDataStoreBarrier();
}
