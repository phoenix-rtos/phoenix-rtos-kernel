/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32L1 basic peripherals control functions
 *
 * Copyright 2018, 2019, 2020 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_STM32L1_H_
#define _PHOENIX_ARCH_STM32L1_H_


#define PCTL_REBOOT_MAGIC 0xaa55aa55UL
#define PINRSRF (1 << 0)
#define PORRSTF (1 << 1)
#define SFTRSTF (1 << 2)
#define IWDGRSTF (1 << 3)
#define WWDGRSTF (1 << 4)
#define LPWRRSTF (1 << 5)


/* STM32L1 peripherals */
enum {
	/* AHB */
	pctl_gpioa = 0, pctl_gpiob, pctl_gpioc, pctl_gpiod, pctl_gpioe, pctl_gpioh, pctl_gpiof,
	pctl_gpiog, pctl_crc = 12, pctl_flash = 15, pctl_dma1 = 24, pctl_dma2, pctl_aes = 27, pctl_fsmc = 30,

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


/* STM32L1 Interrupt numbers */
enum { wwdq_irq = 16, pvd_irq, tamper_stamp_irq, rtc_wkup_irq, flash_irq, rcc_irq,
	exti0_irq, exti1_irq, exti2_irq, exti3_irq, exti4_irq, dma1ch1_irq, dma1ch2_irq,
	dma1ch3_irq, dma1ch4_irq, dma1ch5_irq, dma1ch6_irq, dma1ch7_irq, adc1_irq,
	usbhp_irq, usblp_irq, dac_irq, comp_irq, exti9_5_irq, lcd_irq, tim9_irq, tim10_irq,
	tim11_irq, tim2_irq, tim3_irq, tim4_irq, i2c1_ev_irq, i2c1_er_irq, i2c2_ev_irq,
	i2c2_er_irq, spi1_irq, spi2_irq, usart1_irq, usart2_irq, usart3_irq, exti15_10_irq,
	rtc_alrm_irq, usb_fs_wkup_irq, tim6_irq, tim7_irq, sdio_irq, tim5_irq, spi3_irq,
	uart4_irq, uart5_irq, dma2ch1_irq, dma2ch2_irq, dma2ch3_irq, dma2ch4_irq, dma2ch5_irq,
	comp_acq_irq = 72 };


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_devclk = 0, pctl_cpuclk, pctl_reboot } type;

	union {
		struct {
			unsigned int dev;
			unsigned int state;
		} devclk;

		struct {
			unsigned int hz;
		} cpuclk;

		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;
	};
} __attribute__((packed)) platformctl_t;


#endif
