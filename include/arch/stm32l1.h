/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32L1 basic peripherals control functions
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_STM32L1_H_
#define _PHOENIX_ARCH_STM32L1_H_


/* STM32 peripherals */
enum {
	/* AHB */
	pctl_gpioa = 0, pctl_gpiob, pctl_gpioc, pctl_gpiod, pctl_gpioe, pctl_gpiof, pctl_gpiog,
	pctl_gpioh, pctl_crc = 12, pctl_flash = 15, pctl_dma1 = 24, pctl_dma2, pctl_aes = 27, pctl_fsmc = 30,

	/* APB2 */
	pctl_syscfg = 32, pctl_tim9 = 34, pctl_tim10, pctl_tim11, pctl_adc1 = 41, pctl_sdio = 43,
	pctl_spi1, pctl_usart1 = 46,

	/* APB1 */
	pctl_tim2 = 64, pctl_tim3, pctl_tim4, pctl_tim5, pctl_tim6, pctl_tim7, pctl_lcd = 73, pctl_wwd = 75,
	pctl_spi2 = 78, pctl_spi3, pctl_usart2 = 81, pctl_usart3, pctl_uart4, pctl_uart5, pctl_i2c1,
	pctl_i2c2, pctl_usb, pctl_pwr = 92, pctl_dac, pctl_comp = 95,

	/* MISC */
	pctl_rtc = 96, pctl_msi, pctl_hsi
};


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_devclk = 0, pctl_cpuclk } type;

	union {
		struct {
			unsigned int dev;
			unsigned int state;
		} devclk;

		struct {
			unsigned int hz;
		} cpuclk;
	};
} __attribute__((packed)) platformctl_t;


#endif
