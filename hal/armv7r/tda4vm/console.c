/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (16550-compatible UART)
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/console.h"
#include "hal/cpu.h"

#include "tda4vm.h"
#include "include/arch/armv7r/tda4vm/tda4vm_pins.h"

#include <board_config.h>


#define MCU_UART0_BASE_ADDR  ((void *)0x40a00000)
#define MAIN_UART0_BASE_ADDR ((void *)0x02800000)
#define MAIN_UART1_BASE_ADDR ((void *)0x02810000)
#define MAIN_UART2_BASE_ADDR ((void *)0x02820000)
#define MAIN_UART3_BASE_ADDR ((void *)0x02830000)
#define MAIN_UART4_BASE_ADDR ((void *)0x02840000)
#define MAIN_UART5_BASE_ADDR ((void *)0x02850000)
#define MAIN_UART6_BASE_ADDR ((void *)0x02860000)
#define MAIN_UART7_BASE_ADDR ((void *)0x02870000)
#define MAIN_UART8_BASE_ADDR ((void *)0x02880000)
#define MAIN_UART9_BASE_ADDR ((void *)0x02890000)


#if UART_CONSOLE_KERNEL == 0U
#define UART_RX       UART0_RX
#define UART_TX       UART0_TX
#define UART_BAUDRATE UART0_BAUDRATE
#else
#error "TODO: support other UARTs for console"
#endif


#define MAX_PINS_PER_UART 6
typedef struct {
	volatile u32 *base;
	struct {
		s16 clksel; /* Value of < 0 means this UART has no CLKSEL */
		u8 clksel_val;
	};
	s16 clkdiv; /* Value of < 0 means this UART has no CLKSEL */
	u8 divisor;
	u8 pll;
	u8 hsdiv;
	struct {
		s16 pin; /* Value of < 0 signals end of list */
		u8 muxSetting;
		u8 isTx;
	} pins[MAX_PINS_PER_UART];
} tda4vm_uart_info_t;


static const tda4vm_uart_info_t uart_info[] = {
	{
		.base = MCU_UART0_BASE_ADDR,
		.clksel = clksel_mcu_usart,
		.clksel_val = 0, /* CLKSEL set to MCU_PLL1_HSDIV3_CLKOUT */
		.clkdiv = -1,
		.divisor = 1,
		.pll = clk_mcu_per_pll1,
		.hsdiv = 3,
		.pins = {
			{ pin_mcu_ospi1_d2, 4, 1 },
			{ pin_wkup_gpio0_10, 2, 1 },
			{ pin_wkup_gpio0_12, 0, 1 },
			{ pin_mcu_ospi1_d1, 4, 0 },
			{ pin_wkup_gpio0_11, 2, 0 },
			{ pin_wkup_gpio0_13, 0, 0 },
		},
	},
};


static struct {
	volatile u32 *uart;
} console_common;


/* UART registers */
enum {
	rbr = 0, /* Receiver Buffer Register */
	thr = 0, /* Transmitter Holding Register */
	dll = 0, /* Divisor Latch LSB */
	ier = 1, /* Interrupt Enable Register */
	dlm = 1, /* Divisor Latch MSB */
	iir = 2, /* Interrupt Identification Register */
	fcr = 2, /* FIFO Control Register */
	efr = 2, /* Enhanced feature register */
	lcr = 3, /* Line Control Register */
	mcr = 4, /* Modem Control Register */
	lsr = 5, /* Line Status Register */
	msr = 6, /* Modem Status Register */
	spr = 7, /* Scratch Pad Register */
	/* Below are extensions */
	mdr1 = 8, /* Mode definition register 1 */
	mdr2 = 9, /* Mode definition register 2 */
};


static unsigned char reg_read(unsigned int reg)
{
	return *(console_common.uart + reg);
}


static void reg_write(unsigned int reg, unsigned char val)
{
	*(console_common.uart + reg) = val;
}


void hal_consolePutch(char c)
{
	while ((reg_read(lsr) & 0x20) == 0) {
		/* Wait until TX fifo is empty */
	}

	reg_write(thr, c);
}


static void _hal_consolePrint(const char *s)
{
	for (; *s != '\0'; s++) {
		hal_consolePutch(*s);
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


static u32 console_initClock(const tda4vm_uart_info_t *info)
{
	if (info->clksel >= 0) {
		tda4vm_setClksel(info->clksel, info->clksel_val);
	}

	if (info->clkdiv >= 0) {
		tda4vm_setClkdiv(info->clkdiv, info->divisor);
	}

	return (u32)tda4vm_getFrequency(info->pll, info->hsdiv) / info->divisor;
}


static unsigned int console_calcDivisor(u32 baseClock)
{
	/* Assume we are in UART x16 mode */
	const u32 baud_16 = (16 * UART_BAUDRATE);
	u32 divisor, remainder;
	divisor = baseClock / baud_16;
	remainder = baseClock % baud_16;
	divisor += (remainder >= (baud_16 / 2)) ? 1 : 0;

	/* On this platform DLH register only holds 6 bits - so limit to 14 bits */
	return (divisor >= (1 << 14)) ? ((1 << 14) - 1) : divisor;
}


static void console_setPin(const tda4vm_uart_info_t *info, u32 pin)
{
	int muxSetting, isTx;
	tda4vm_pinConfig_t cfg;
	for (unsigned int i = 0; i < MAX_PINS_PER_UART; i++) {
		if (info->pins[i].pin < 0) {
			return;
		}

		if (info->pins[i].pin == pin) {
			muxSetting = info->pins[i].muxSetting;
			isTx = info->pins[i].isTx;
			cfg.debounce_idx = 0;
			cfg.mux = muxSetting & 0xff;
			if (isTx) {
				cfg.flags = TDA4VM_GPIO_PULL_DISABLE;
			}
			else {
				cfg.flags = TDA4VM_GPIO_RX_EN | TDA4VM_GPIO_PULL_DISABLE;
			}

			tda4vm_setPinConfig(pin, &cfg);
			return;
		}
	}
}


void _hal_consoleInit(void)
{
	unsigned int n = UART_CONSOLE_KERNEL;
	u32 baseClock, divisor;
	const tda4vm_uart_info_t *info;

	if (n >= sizeof(uart_info) / sizeof(uart_info[0])) {
		return;
	}

	info = &uart_info[n];
	console_common.uart = info->base;
	while ((reg_read(lsr) & 0x40) == 0) {
		/* Wait until all data is shifted out */
	}

	baseClock = console_initClock(info);
	console_setPin(info, UART_RX);
	console_setPin(info, UART_TX);

	/* Put into UART x16 mode */
	reg_write(mdr1, 0x0);

	/* Enable enhanced functions */
	reg_write(lcr, 0xbf);
	reg_write(efr, 1 << 4);
	reg_write(lcr, 0x0);

	/* Set DTR and RTS */
	reg_write(mcr, 0x03);

	/* Enable and configure FIFOs */
	reg_write(fcr, 0xa7);

	/* Set speed */
	divisor = console_calcDivisor(baseClock);
	reg_write(lcr, 0x80);
	reg_write(dll, divisor & 0xff);
	reg_write(dlm, (divisor >> 8) & 0xff);

	/* Set data format */
	reg_write(lcr, 0x03);

	/* Disable interrupts */
	reg_write(ier, 0x00);
}
