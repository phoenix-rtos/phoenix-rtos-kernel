/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32U3 basic peripherals control functions
 * Based on stm32u3c5xx.h by STMicroelectronics
 *
 * Copyright 2026 Apator Metrix
 * Authors: Mateusz Karcz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_ARCH_STM32U3_H_
#define _PH_ARCH_STM32U3_H_


#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* Clock Enable Register relative to AHB1ENR1, encoded with device bit */
#define PCTL_DEVID(rcc_reg, pos) (((((rcc_reg) - 0x088U) >> 2U) << 5U) | (pos))

/* STM32U3 device identifiers */
enum {
	pctl_gpdma1 = PCTL_DEVID(0x088U, 0U),
	pctl_adf1 = PCTL_DEVID(0x088U, 3U),
	pctl_hsp1 = PCTL_DEVID(0x088U, 4U),
	pctl_flash = PCTL_DEVID(0x088U, 8U),
	pctl_crc = PCTL_DEVID(0x088U, 12U),
	pctl_tsc = PCTL_DEVID(0x088U, 16U),
	pctl_ramcfg = PCTL_DEVID(0x088U, 17U),
	pctl_gtzc1 = PCTL_DEVID(0x088U, 24U),
	pctl_sram4 = PCTL_DEVID(0x088U, 30U),
	pctl_sram1 = PCTL_DEVID(0x088U, 31U),
	pctl_gpioa = PCTL_DEVID(0x08cU, 0U),
	pctl_gpiob = PCTL_DEVID(0x08cU, 1U),
	pctl_gpioc = PCTL_DEVID(0x08cU, 2U),
	pctl_gpiod = PCTL_DEVID(0x08cU, 3U),
	pctl_gpioe = PCTL_DEVID(0x08cU, 4U),
	pctl_gpiof = PCTL_DEVID(0x08cU, 5U),
	pctl_gpiog = PCTL_DEVID(0x08cU, 6U),
	pctl_gpioh = PCTL_DEVID(0x08cU, 7U),
	pctl_adc12 = PCTL_DEVID(0x08cU, 10U),
	pctl_dac1 = PCTL_DEVID(0x08cU, 11U),
	pctl_aes = PCTL_DEVID(0x08cU, 16U),
	pctl_hash = PCTL_DEVID(0x08cU, 17U),
	pctl_rng = PCTL_DEVID(0x08cU, 18U),
	pctl_pka = PCTL_DEVID(0x08cU, 19U),
	pctl_saes = PCTL_DEVID(0x08cU, 20U),
	pctl_ccb = PCTL_DEVID(0x08cU, 21U),
	pctl_sdmmc1 = PCTL_DEVID(0x08cU, 27U),
	pctl_sram2 = PCTL_DEVID(0x08cU, 30U),
	pctl_sram3 = PCTL_DEVID(0x08cU, 31U),
	pctl_octospi1 = PCTL_DEVID(0x090U, 4U),
	pctl_pwr = PCTL_DEVID(0x094U, 2U),
	pctl_tim2 = PCTL_DEVID(0x09cU, 0U),
	pctl_tim3 = PCTL_DEVID(0x09cU, 1U),
	pctl_tim4 = PCTL_DEVID(0x09cU, 2U),
	pctl_tim6 = PCTL_DEVID(0x09cU, 4U),
	pctl_tim7 = PCTL_DEVID(0x09cU, 5U),
	pctl_spi3 = PCTL_DEVID(0x09cU, 8U),
	pctl_spi4 = PCTL_DEVID(0x09cU, 9U),
	pctl_wwdg = PCTL_DEVID(0x09cU, 11U),
	pctl_iwdg = PCTL_DEVID(0x09cU, 12U), /* This value is only valid for _stm32_dbgmcuStopTimerInDebug */
	pctl_spi2 = PCTL_DEVID(0x09cU, 14U),
	pctl_usart2 = PCTL_DEVID(0x09cU, 17U),
	pctl_usart3 = PCTL_DEVID(0x09cU, 18U),
	pctl_uart4 = PCTL_DEVID(0x09cU, 19U),
	pctl_uart5 = PCTL_DEVID(0x09cU, 20U),
	pctl_i2c1 = PCTL_DEVID(0x09cU, 21U),
	pctl_i2c2 = PCTL_DEVID(0x09cU, 22U),
	pctl_i3c1 = PCTL_DEVID(0x09cU, 23U),
	pctl_crs = PCTL_DEVID(0x09cU, 24U),
	pctl_opamp = PCTL_DEVID(0x09cU, 28U),
	pctl_vref = PCTL_DEVID(0x09cU, 29U),
	pctl_rtcapb = PCTL_DEVID(0x09cU, 30U),
	pctl_i2c4 = PCTL_DEVID(0x0a0U, 1U),
	pctl_lptim2 = PCTL_DEVID(0x0a0U, 5U),
	pctl_fdcan = PCTL_DEVID(0x0a0U, 9U),
	pctl_tim1 = PCTL_DEVID(0x0a4U, 11U),
	pctl_spi1 = PCTL_DEVID(0x0a4U, 12U),
	pctl_tim8 = PCTL_DEVID(0x0a4U, 13U),
	pctl_usart1 = PCTL_DEVID(0x0a4U, 14U),
	pctl_tim12 = PCTL_DEVID(0x0a4U, 15U),
	pctl_tim15 = PCTL_DEVID(0x0a4U, 16U),
	pctl_tim16 = PCTL_DEVID(0x0a4U, 17U),
	pctl_tim17 = PCTL_DEVID(0x0a4U, 18U),
	pctl_sai1 = PCTL_DEVID(0x0a4U, 21U),
	pctl_usb1 = PCTL_DEVID(0x0a4U, 24U),
	pctl_i3c2 = PCTL_DEVID(0x0a4U, 27U),
	pctl_syscfg = PCTL_DEVID(0x0a8U, 1U),
	pctl_lpuart1 = PCTL_DEVID(0x0a8U, 6U),
	pctl_i2c3 = PCTL_DEVID(0x0a8U, 7U),
	pctl_lptim1 = PCTL_DEVID(0x0a8U, 11U),
	pctl_lptim3 = PCTL_DEVID(0x0a8U, 12U),
	pctl_lptim4 = PCTL_DEVID(0x0a8U, 13U),
	pctl_comp = PCTL_DEVID(0x0a8U, 15U),
	pctl_rtc = PCTL_DEVID(0x110U, 15U),
};


/* STM32U3 independent device clocks */
enum ipclks {
	pctl_ipclk_usart1sel = 0,
	pctl_ipclk_usart3sel,
	pctl_ipclk_uart4sel,
	pctl_ipclk_uart5sel,
	pctl_ipclk_i3c1sel,
	pctl_ipclk_i2c1sel,
	pctl_ipclk_i2c2sel,
	pctl_ipclk_i3c2sel,
	pctl_ipclk_spi2sel,
	pctl_ipclk_lptim2sel,
	pctl_ipclk_spi1sel,
	pctl_ipclk_systicksel,
	pctl_ipclk_fdcansel,
	pctl_ipclk_iclksel,
	pctl_ipclk_adf1sel,
	pctl_ipclk_spi3sel,
	pctl_ipclk_sai1sel,
	pctl_ipclk_spi4sel,
	pctl_ipclk_i2c4sel,
	pctl_ipclk_rngsel,
	pctl_ipclk_adcdacsel,
	pctl_ipclk_dac1shsel,
	pctl_ipclk_octospisel,
	pctl_ipclk_usart2sel,
	pctl_ipclk_lpuart1sel,
	pctl_ipclk_i2c3sel,
	pctl_ipclk_lptim34sel,
	pctl_ipclk_lptim1sel,
	pctl_ipclks_count
};


/* STM32U3 Interrupt numbers */
enum {
	wwdg_irq = 16,
	pvd_pvm_irq,
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
	iwdg_irq,
	saes_irq,
	gpdma1_ch0_irq,
	gpdma1_ch1_irq,
	gpdma1_ch2_irq,
	gpdma1_ch3_irq,
	gpdma1_ch4_irq,
	gpdma1_ch5_irq,
	gpdma1_ch6_irq,
	gpdma1_ch7_irq,
	adc1_irq,
	dac1_irq,
	fdcan1_it0_irq,
	fdcan1_it1_irq,
	tim1_brk_terr_ierr_irq,
	tim1_up_irq,
	tim1_trg_com_dir_idx_irq,
	tim1_cc_irq,
	tim2_irq,
	tim3_irq,
	tim4_irq,
	tim6_irq = 65,
	tim7_irq,
	tim12_irq,
	i3c1_ev_irq = 69,
	i3c1_er_irq,
	i2c1_ev_irq,
	i2c1_er_irq,
	i2c2_ev_irq,
	i2c2_er_irq,
	spi1_irq,
	spi2_irq,
	usart1_irq,
	usart2_irq,
	usart3_irq,
	uart4_irq,
	uart5_irq,
	lpuart1_irq,
	lptim1_irq,
	lptim2_irq,
	tim15_irq,
	tim16_irq,
	tim17_irq,
	comp_irq,
	usb_fs_irq,
	crs_irq,
	octospi1_irq = 92,
	hsp1_irq,
	sdmmc1_irq,
	gpdma1_ch8_irq = 96,
	gpdma1_ch9_irq,
	gpdma1_ch10_irq,
	gpdma1_ch11_irq,
	i2c3_ev_irq = 104,
	i2c3_er_irq,
	sai1_irq,
	tsc_irq = 108,
	aes_irq,
	rng_irq,
	fpu_irq,
	hash_irq,
	pka_irq,
	lptim3_irq,
	spi3_irq,
	i3c2_ev_irq,
	i3c2_er_irq,
	tim8_brk_terr_ierr_irq,
	tim8_up_irq,
	tim8_trg_com_dir_idx_irq,
	tim8_cc_irq,
	icache_irq = 123,
	lptim4_irq = 126,
	adf1_irq = 128,
	adc2_irq,
	fdcan2_it0_irq,
	fdcan2_it1_irq,
	i2c4_ev_irq,
	i2c4_er_irq,
	spi4_irq = 135,
	pwr_irq = 139,
	pwr_s_irq,
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
		pctl_dmaPermissions,
		pctl_dmaLinkBaseAddr,
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
			int dev; /* one of pctl_gpdma* enum values */
			unsigned int channel;
			int privileged; /* 1 - set to privileged, 0 - no change, -1 - set to unprivileged */
			int secure;     /* 1 - set to secure, 0 - no change, -1 - set to non-secure */
			int lock;       /* 1 - lock from changes until reset, 0 - no change */
		} dmaPermissions;
		struct {
			int dev; /* one of pctl_gpdma* enum values */
			unsigned int channel;
			unsigned int addr;
		} dmaLinkBaseAddr;
		struct {
			unsigned int ipclk;
			unsigned int setting;
		} ipclk;
		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;
	};
} __attribute__((packed)) platformctl_t;


#endif
