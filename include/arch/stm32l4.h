/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32L4 basic peripherals control functions
 *
 * Copyright 2019 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_STM32L4_H_
#define _PHOENIX_ARCH_STM32L4_H_


#define PCTL_REBOOT_MAGIC 0xaa55aa55UL
/*
#define PINRSRF (1 << 0)
#define PORRSTF (1 << 1)
#define SFTRSTF (1 << 2)
#define IWDGRSTF (1 << 3)
#define WWDGRSTF (1 << 4)
#define LPWRRSTF (1 << 5)
*/

/* STM32 peripherals */
enum {
	/* AHB1 */
	pctl_dma1 = 0, pctl_dma2, pctl_flash = 8, pctl_crc = 12, pctl_tsc = 16, pctl_dma2d,

	/* AHB2 */
	pctl_gpioa = 32, pctl_gpiob, pctl_gpioc, pctl_gpiod, pctl_gpioe, pctl_gpiof, pctl_gpiog, pctl_gpioh, pctl_gpioi,
	pctl_otgfs = 32 + 12, pctl_adc, pctl_dcmi, pctl_aes = 32 + 16, pctl_hash, pctl_rng,

	/* AHB3 */
	pctl_fmc = 64, pctl_qspi = 64 + 8,

	/* APB1_1 */
	pctl_tim2 = 96, pctl_tim3, pctl_tim4, pctl_tim5, pctl_tim6, pctl_tim7, pctl_lcd = 96 + 9, pctl_rtcapb,
	pctl_wwdg, pctl_spi2 = 96 + 14, pctl_spi3, pctl_usart2 = 96 + 17, pctl_usart3, pctl_uart4, pctl_i2c1,
	pctl_i2c2, pctl_i2c3, pctl_crs, pctl_can1, pctl_can2, pctl_pwr = 96 + 28, pctl_dac1, pctl_opamp, pctl_lptim1,

	/* APB1_2 */
	pctl_lpuart1 = 128, pctl_i2c4, pctl_swpmi1, pctl_lptim2 = 128 + 5,

	/* APB2 */
	pctl_syscfg = 160, pctl_fw = 160 + 7, pctl_sdmmc1 = 160 + 10, pctl_tim1, pctl_spi1, pctl_tim8, pctl_usart1,
	pctl_tim15, pctl_tim16, pctl_tim17, pctl_sai1 = 160 + 21, pctl_sai2, pctl_dfsdm1 = 160 + 24,

	/* MISC */
	pctl_rtc = 192, pctl_msi, pctl_hsi
};


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
