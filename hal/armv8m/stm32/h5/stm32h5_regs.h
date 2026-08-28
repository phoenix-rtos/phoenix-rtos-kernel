/*
 * Phoenix-RTOS
 *
 * plo - operating system loader
 *
 * Peripheral register definitions for STM32H5 platform
 *
 * Copyright 2025, 2026 Phoenix Systems
 * Author: Jacek Maksymowicz, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _STM32H5_REGS_H_
#define _STM32H5_REGS_H_

enum rcc_regs {
	rcc_cr = 0x0,
	rcc_hsicfgr = 0x10 / 4,
	rcc_crrcr,
	rcc_csicfgr,
	rcc_cfgr1,
	rcc_cfgr2,
	rcc_pll1cfgr = 0x28 / 4,
	rcc_pll2cfgr,
	rcc_pll3cfgr,
	rcc_pll1divr,
	rcc_pll1fracr,
	rcc_pll2divr,
	rcc_pll2fracr,
	rcc_pll3divr,
	rcc_pll3fracr,
	rcc_cier = 0x50 / 4,
	rcc_cifr,
	rcc_cicr,
	rcc_ahb1rstr = 0x60 / 4,
	rcc_ahb2rstr,
	rcc_ahb4rstr,
	rcc_apb1lrstr = 0x74 / 4,
	rcc_apb1hrstr,
	rcc_apb2rstr,
	rcc_apb3rstr,
	rcc_ahb1enr = 0x88 / 4,
	rcc_ahb2enr,
	rcc_ahb4enr = 0x94 / 4,
	rcc_apb1lenr = 0x9c / 4,
	rcc_apb1henr = 0xa0 / 4,
	rcc_apb2enr,
	rcc_apb3enr,
	rcc_ahb1lpenr = 0xb0 / 4,
	rcc_ahb2lpenr,
	rcc_ahb4lpenr = 0xbc / 4,
	rcc_apb1llpenr = 0xc4 / 4,
	rcc_apb1hlpenr,
	rcc_apb2lpenr,
	rcc_apb3lpenr,
	rcc_ccipr1 = 0xd8 / 4,
	rcc_ccipr2,
	rcc_ccipr3,
	rcc_ccipr4,
	rcc_ccipr5,
	rcc_bdcr = 0xf0 / 4,
	rcc_rsr,
	rcc_seccfgr = 0x110 / 4,
	rcc_privcfgr
};


enum gpio_regs {
	gpio_moder = 0x0,
	gpio_otyper,
	gpio_ospeedr,
	gpio_pupdr,
	gpio_idr,
	gpio_odr,
	gpio_bsrr,
	gpio_lckr,
	gpio_afrl,
	gpio_afrh,
	gpio_brr,
	gpio_hslvr,
	gpio_seccfgr
};


enum pwr_regs {
	pwr_pmcr = 0x0,
	pwr_pmsr,
	pwr_voscr = 0x10 / 4,
	pwr_vossr,
	pwr_bdcr = 0x20 / 4,
	pwr_dbpcr,
	pwr_bdsr,
	pwr_ucpdr,
	pwr_sccr,
	pwr_vmcr,
	pwr_usbscr,
	pwr_vmsr,
	pwr_wuscr,
	pwr_wusr,
	pwr_wucr,
	pwr_ioretr = 0x50 / 4,
	pwr_seccfgr = 0x100 / 4,
	pwr_privcfgr
};


enum rtc_regs {
	rtc_tr = 0x0,
	rtc_dr,
	rtc_ssr,
	rtc_icsr,
	rtc_prer,
	rtc_wutr,
	rtc_cr,
	rtc_privcfgr,
	rtc_seccfgr,
	rtc_wpr,
	rtc_calr,
	rtc_shiftr,
	rtc_tstr,
	rtc_tsdr,
	rtc_tsssr,
	rtc_alrmar = 0x10,
	rtc_alrmassr,
	rtc_alrmbr,
	rtc_alrmbssr,
	rtc_sr,
	rtc_misr,
	rtc_smisr,
	rtc_scr,
	rtc_alrabinr = 0x1c,
	rtc_alrbbinr,
};


enum iwdg_regs {
	iwdg_kr = 0x0,
	iwdg_pr,
	iwdg_rlr,
	iwdg_sr,
	iwdg_winr,
	iwdg_ewcr,
	iwdg_icr,
};


enum {
	gpdma_seccfgr = 0x0,
	gpdma_privcfgr,
	gpdma_rcfglockr,
	gpdma_misr,
	gpdma_smisr,
	gpdma_cxlbar = 0x14,
};

#endif /* _STM32N6_REGS_H_ */
