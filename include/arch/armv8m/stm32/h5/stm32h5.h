/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32H5 basic peripherals control functions
 *
 * Copyright 2025, 2026 Phoenix Systems
 * Author: Jacek Maksymowicz, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_ARCH_STM32H5_H_
#define _PH_ARCH_STM32H5_H_


#define PCTL_REBOOT_MAGIC 0xaa55aa55UL


/* STM32H5 device identifiers */
enum {
	/* AHB1 */
	pctl_gpdma1 = 0 + 0,
	pctl_gpdma2,
	pctl_flitf = 0 + 8,
	pctl_crc = 0 + 12,
	pctl_cordic = 0 + 14,
	pctl_fmac,
	pctl_ramcfg = 0 + 17,
	pctl_eth = 0 + 19,
	pctl_ethtx,
	pctl_ethrx,
	pctl_tzsc1 = 0 + 24,
	pctl_bkpram = 0 + 28,
	pctl_dcache = 0 + 30,
	pctl_sram1,

	/* AHB2 */
	pctl_gpioa = 32 + 0,
	pctl_gpiob,
	pctl_gpioc,
	pctl_gpiod,
	pctl_gpioe,
	pctl_gpiof,
	pctl_gpiog,
	pctl_gpioh,
	pctl_gpioi,
	pctl_adc = 32 + 10,
	pctl_dac1,
	pctl_dcmi_pssi,
	pctl_aes = 32 + 16,
	pctl_hash,
	pctl_rng,
	pctl_pka,
	pctl_saes,
	pctl_sram2 = 32 + 30,
	pctl_sram3,

	/* AHB4 */
	pctl_otfdec1 = 64 + 7,
	pctl_sdmmc1 = 64 + 11,
	pctl_sdmmc2,
	pctl_fmc = 64 + 16,
	pctl_octospi1 = 64 + 20,

	/* APB1 low */
	pctl_tim2 = 96 + 0,
	pctl_tim3,
	pctl_tim4,
	pctl_tim5,
	pctl_tim6,
	pctl_tim7,
	pctl_tim12,
	pctl_tim13,
	pctl_tim14,
	pctl_wwdg = 96 + 11,
	pctl_spi2 = 96 + 14,
	pctl_spi3,
	pctl_usart2 = 96 + 17,
	pctl_usart3,
	pctl_uart4,
	pctl_uart5,
	pctl_i2c1,
	pctl_i2c2,
	pctl_i3c1,
	pctl_crs,
	pctl_usart6,
	pctl_usart10,
	pctl_usart11,
	pctl_cec,
	pctl_uart7 = 96 + 30,
	pctl_uart8,

	/* APB1 high */
	pctl_uart9 = 128 + 0,
	pctl_uart12,
	pctl_dts = 128 + 3,
	pctl_lptim2 = 128 + 5,
	pctl_fdcan = 128 + 9,
	pctl_ucpd1 = 128 + 23,

	/* APB2 */
	pctl_tim1 = 160 + 11,
	pctl_spi1,
	pctl_tim8,
	pctl_usart1,
	pctl_tim15 = 160 + 16,
	pctl_tim16,
	pctl_tim17,
	pctl_spi4,
	pctl_spi6,
	pctl_sai1,
	pctl_sai2,
	pctl_usb = 160 + 24,

	/* APB3 */
	pctl_sbs = 192 + 1,
	pctl_spi5 = 192 + 5,
	pctl_lpuart1,
	pctl_i2c3,
	pctl_i2c4,
	pctl_i3c2,
	pctl_lptim1 = 192 + 11,
	pctl_lptim3,
	pctl_lptim4,
	pctl_lptim5,
	pctl_lptim6,
	pctl_vref = 192 + 20,
	pctl_rtc
};


/* STM32H5 independent device clocks */
enum ipclks {
	pctl_ipclk_usart1sel,
	pctl_ipclk_usart2sel,
	pctl_ipclk_usart3sel,
	pctl_ipclk_uart4sel,
	pctl_ipclk_uart5sel,
	pctl_ipclk_usart6sel,
	pctl_ipclk_uart7sel,
	pctl_ipclk_uart8sel,
	pctl_ipclk_uart9sel,
	pctl_ipclk_usart10sel,
	pctl_ipclk_timicsel,
	pctl_ipclk_usart11sel,
	pctl_ipclk_uart12sel,
	pctl_ipclk_lptim1sel,
	pctl_ipclk_lptim2sel,
	pctl_ipclk_lptim3sel,
	pctl_ipclk_lptim4sel,
	pctl_ipclk_lptim5sel,
	pctl_ipclk_lptim6sel,
	pctl_ipclk_spi1sel,
	pctl_ipclk_spi2sel,
	pctl_ipclk_spi3sel,
	pctl_ipclk_spi4sel,
	pctl_ipclk_spi5sel,
	pctl_ipclk_spi6sel,
	pctl_ipclk_lpuart1sel,
	pctl_ipclk_octospi1sel,
	pctl_ipclk_systicksel,
	pctl_ipclk_usbsel,
	pctl_ipclk_sdmmc1sel,
	pctl_ipclk_sdmmc2sel,
	pctl_ipclk_i2c1sel,
	pctl_ipclk_i2c2sel,
	pctl_ipclk_i2c3sel,
	pctl_ipclk_i2c4sel,
	pctl_ipclk_i3c1sel,
	pctl_ipclk_i3c2sel,
	pctl_ipclk_adcdacsel,
	pctl_ipclk_dacsel,
	pctl_ipclk_rngsel,
	pctl_ipclk_cecsel,
	pctl_ipclk_fdcansel,
	pctl_ipclk_sai1sel,
	pctl_ipclk_sai2sel,
	pctl_ipclk_ckpersel,
	pctl_ipclk_count
};


/* STM32N6 Interrupt numbers */
enum{
	wwdg_irq = 0 + 16,
	pvd_avd_irq,
	rtc_irq,
	rtc_s_irq,
	tamp_irq,
	ramcfg_irq,
	flash_irq,
	flash_s_irq,
	gtzc_irq,
	rcc_irq,
	rcc_s_irq,
	exti0_irq,
	exti1_irq,
	exti2_irq,
	exti3_irq,
	exti4_irq,
	exti5_irq,
	exti6_irq,
	exti7_irq,
	exti8_irq,
	exti9_irq,
	exti10_irq,
	exti11_irq,
	exti12_irq,
	exti13_irq,
	exti14_irq,
	exti15_irq,
	gpdma1_ch0_irq,
	gpdma1_ch1_irq,
	gpdma1_ch2_irq,
	gpdma1_ch3_irq,
	gpdma1_ch4_irq,
	gpdma1_ch5_irq,
	gpdma1_ch6_irq,
	gpdma1_ch7_irq,
	iwdg_irq,
	adc1_irq = 37 + 16,
	dac1_irq,
	fdcan1_it0_irq,
	fdcan1_it1_irq,
	tim1_brk_irq,
	tim1_up_irq,
	tim1_trg_com_irq,
	tim1_cc_irq,
	tim2_irq,
	tim3_irq,
	tim4_irq,
	tim5_irq,
	tim6_irq,
	tim7_irq,
	i2c1_ev_irq,
	i2c1_er_irq,
	i2c2_ev_irq,
	i2c2_er_irq,
	spi1_irq,
	spi2_irq,
	spi3_irq,
	usart1_irq,
	usart2_irq,
	usart3_irq,
	uart4_irq,
	uart5_irq,
	lpuart1_irq,
	lptim1_irq,
	tim8_brk_irq,
	tim8_up_irq,
	tim8_trg_com_irq,
	tim8_cc_irq,
	adc2_irq,
	lptim2_irq,
	tim15_irq,
	tim16_irq,
	tim17_irq,
	usb_fs_irq,
	crs_irq,
	ucpd1_irq,
	fmc_irq,
	octospi1_irq,
	sdmmc1_irq,
	i2c3_ev_irq,
	i2c3_er_irq,
	spi4_irq,
	spi5_irq,
	spi6_irq,
	usart6_irq,
	usart10_irq,
	usart11_irq,
	sai1_irq,
	sai2_irq,
	gpdma2_ch0_irq,
	gpdma2_ch1_irq,
	gpdma2_ch2_irq,
	gpdma2_ch3_irq,
	gpdma2_ch4_irq,
	gpdma2_ch5_irq,
	gpdma2_ch6_irq,
	gpdma2_ch7_irq,
	uart7_irq,
	uart8_irq,
	uart9_irq,
	uart12_irq,
	sdmmc2_irq,
	fpu_irq,
	icache_irq,
	dcache_irq,
	eth_irq,
	eth_wkup_irq,
	dcmi_pssi_irq,
	fdcan2_it0_irq,
	fdcan2_it1_irq,
	cordic_irq,
	fmac_irq,
	dts_irq,
	rng_irq,
	otfdec1_irq,
	hash_irq = 117 + 16,
	pka_irq,
	cec_irq,
	tim12_irq,
	tim13_irq,
	tim14_irq,
	i3c1_ev_irq,
	i3c1_er_irq,
	i2c4_ev_irq,
	i2c4_er_irq,
	lptim3_irq,
	lptim4_irq,
	lptim5_irq,
	lptim6_irq
};


typedef struct {
	enum {
		pctl_set = 0,
		pctl_get,
	} action;

	enum {
		pctl_devclk = 0,
		pctl_cpuclk,
		pctl_ipclk, /* Independent peripheral clock settings (muxes and dividers) */
		pctl_reboot,
		pctl_cleanInvalDCache,
		pctl_cleanDCache,
		pctl_invalDCache,
		pctl_cleanInvalAXICache,
	} type;

	union {
		struct {
			int dev;              /* one of pctl_* enum values */
			unsigned int state;   /* State in Run and Sleep modes: 1 - clock enabled, 0 - clock disabled */
			unsigned int lpState; /* State in Sleep mode: 1 - enabled, 0 - disabled */
		} devclk;
		struct {
			unsigned int hz;
		} cpuclk;
		struct {
			unsigned int ipclk;
			unsigned int setting;
		} ipclk;
		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;
		struct {
			void *addr;
			unsigned int sz;
		} opDCache;
	};
} __attribute__((packed)) platformctl_t;

#endif
