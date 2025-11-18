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

#include "hal/sparcv8leon/sparcv8leon.h"
#include "hal/console.h"
#include "lib/assert.h"
#include "gaisler.h"

#include <arch/pmap.h>

#include <board_config.h>


/* parasoft-begin-suppress MISRAC2012-RULE_20_7 "CONCAT{,_} is used for creation of preprocessor identifiers" */
#define CONCAT_(a, b) a##b
#define CONCAT(a, b)  CONCAT_(a, b)
/* parasoft-end-suppress MISRAC2012-RULE_20_7 */


/* UART control bits */
#define TX_EN (1U << 1)

/* UART status bits */
#define TX_FIFO_FULL (1UL << 9)

/* Console config */
#define CONSOLE_RX        CONCAT(UART, CONCAT(UART_CONSOLE_KERNEL, _RX))
#define CONSOLE_TX        CONCAT(UART, CONCAT(UART_CONSOLE_KERNEL, _TX))
#define CONSOLE_CGU       CONCAT(cgudev_apbuart, UART_CONSOLE_KERNEL)
#define CONSOLE_BAUDRATE  UART_BAUDRATE
#define UART_CONSOLE_BASE CONCAT(UART, CONCAT(UART_CONSOLE_KERNEL, _BASE))


enum {
	uart_data = 0, /* Data register           : 0x00 */
	uart_status,   /* Status register         : 0x04 */
	uart_ctrl,     /* Control register        : 0x08 */
	uart_scaler,   /* Scaler reload register  : 0x0c */
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
	(void)gaisler_setIomuxCfg(CONSOLE_TX, 0x1, 0, 0);
	(void)gaisler_setIomuxCfg(CONSOLE_RX, 0x1, 0, 0);
}

#elif defined(__CPU_GR740)

static void console_cguClkEnable(void)
{
	_gr740_cguClkEnable(CONSOLE_CGU);
}


static int console_cguClkStatus(void)
{
	return _gr740_cguClkStatus(CONSOLE_CGU);
}


static void console_iomuxCfg(void)
{
	(void)gaisler_setIomuxCfg(CONSOLE_TX, iomux_alternateio, 0, 0);
	(void)gaisler_setIomuxCfg(CONSOLE_RX, iomux_alternateio, 0, 0);
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
	for (; *s != '\0'; s++) {
		hal_consolePutch(*s);
	}

	/* Wait until TX fifo is empty */
	while ((*(halconsole_common.uart + uart_status) & TX_FIFO_FULL) != 0U) {
	}
}


static u32 _hal_consoleCalcScaler(u32 baud)
{
	u32 scaler = 0;

	scaler = ((u32)SYSCLK_FREQ / (baud * 8U + 7U));

	return scaler;
}


void hal_consolePutch(char c)
{
	/* Wait until TX fifo is empty */
	while ((*(halconsole_common.uart + uart_status) & TX_FIFO_FULL) != 0U) {
	}
	*(halconsole_common.uart + uart_data) = (unsigned char)c;
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
		/* No action required*/
	}

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void _hal_consoleInit(void)
{
	halconsole_common.uart = _pmap_halMapDevice(PAGE_ALIGN(UART_CONSOLE_BASE), PAGE_OFFS(UART_CONSOLE_BASE), SIZE_PAGE);
	LIB_ASSERT_ALWAYS(halconsole_common.uart != NULL, "failed to map UART device");

	*(halconsole_common.uart + uart_ctrl) = 0;
	hal_cpuDataStoreBarrier();

	console_iomuxCfg();

	if (console_cguClkStatus() == 0) {
		console_cguClkEnable();
	}

	/* Clear UART FIFO */
	while ((*(halconsole_common.uart + uart_status) & (1U << 0)) != 0U) {
		(void)*(halconsole_common.uart + uart_data);
	}
	*(halconsole_common.uart + uart_scaler) = _hal_consoleCalcScaler(CONSOLE_BAUDRATE);
	hal_cpuDataStoreBarrier();
	*(halconsole_common.uart + uart_ctrl) = TX_EN;
	hal_cpuDataStoreBarrier();
}
