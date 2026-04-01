/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Peripheral register definitions for STM32U3 platform
 * Based on stm32u3c5xx.h by STMicroelectronics
 *
 * Copyright 2026 Apator Metrix
 * Authors: Mateusz Karcz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_STM32U3_REGS_H_
#define _PH_STM32U3_REGS_H_

enum rcc_regs {
	rcc_cr = 0x0U,
	rcc_icscr1 = 0x2U,
	rcc_icscr2,
	rcc_icscr3,
	rcc_crrcr,
	rcc_cfgr1 = 0x7U,
	rcc_cfgr2,
	rcc_cfgr3,
	rcc_cfgr4,
	rcc_cier = 0x14U,
	rcc_cifr,
	rcc_cicr,
	rcc_ahb1rstr1 = 0x18U,
	rcc_ahb2rstr1,
	rcc_ahb2rstr2,
	rcc_apb1rstr1 = 0x1dU,
	rcc_apb1rstr2,
	rcc_apb2rstr,
	rcc_apb3rstr,
	rcc_ahb1enr1 = 0x22U,
	rcc_ahb2enr1,
	rcc_ahb2enr2,
	rcc_ahb1enr2,
	rcc_apb1enr1 = 0x27U,
	rcc_apb1enr2,
	rcc_apb2enr,
	rcc_apb3enr,
	rcc_ahb1slpenr1 = 0x2cU,
	rcc_ahb2slpenr1,
	rcc_ahb2slpenr2,
	rcc_ahb1slpenr2,
	rcc_apb1slpenr1 = 0x31U,
	rcc_apb1slpenr2,
	rcc_apb2slpenr,
	rcc_apb3slpenr,
	rcc_ahb1stpenr1 = 0x36U,
	rcc_ahb2stpenr1,
	rcc_apb1stpenr1 = 0x3bU,
	rcc_apb1stpenr2,
	rcc_apb2stpenr,
	rcc_apb3stpenr,
	rcc_ccipr1 = 0x40U,
	rcc_ccipr2,
	rcc_ccipr3,
	rcc_bdcr = 0x44U,
	rcc_csr,
	rcc_seccfgr = 0x4cU,
	rcc_privcfgr,
};


enum gpio_regs {
	gpio_moder = 0x0U,
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
	gpio_seccfgr,
};


enum pwr_regs {
	pwr_cr1 = 0x0U,
	pwr_cr2,
	pwr_cr3,
	pwr_vosr,
	pwr_svmcr,
	pwr_wucr1,
	pwr_wucr2,
	pwr_wucr3,
	pwr_bdcr = 0x9U,
	pwr_dbpr,
	pwr_seccfgr = 0xcU,
	pwr_privcfgr,
	pwr_sr,
	pwr_svmsr,
	pwr_wusr = 0x11U,
	pwr_wuscr,
	pwr_apcr,
	pwr_pucra,
	pwr_pdcra,
	pwr_pucrb,
	pwr_pdcrb,
	pwr_pucrc,
	pwr_pdcrc,
	pwr_pucrd,
	pwr_pdcrd,
	pwr_pucre,
	pwr_pdcre,
	pwr_pucrg = 0x20U,
	pwr_pdcrg,
	pwr_pucrh,
	pwr_pdcrh,
	pwr_i3cpucr1 = 0x2cU,
	pwr_i3cpucr2,
};


enum rtc_regs {
	rtc_tr = 0,
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
	rtc_tamptscr = 0x19,
	rtc_tsidr,
	rtc_alrabinr = 0x1c,
	rtc_alrbbinr,
};


enum iwdg_regs {
	iwdg_kr = 0x0U,
	iwdg_pr,
	iwdg_rlr,
	iwdg_sr,
	iwdg_winr,
	iwdg_ewcr,
};

enum syscfg_regs {
	syscfg_seccfgr = 0x0U,
	syscfg_cfgr1,
	syscfg_fpuimr,
	syscfg_cnslckr,
	syscfg_cslckr,
	syscfg_cfgr2,
	syscfg_cccsr = 0x7U,
	syscfg_ccvr,
	syscfg_cccr,
	syscfg_rsscmdr = 0xbU,
};

enum ramcfg_regs {
	ramcfg_sram1cr = 0x0U,
	ramcfg_sram1ier,
	ramcfg_sram1isr,
	ramcfg_sram1pear = 0x4U,
	ramcfg_sram1icr,
	ramcfg_sram1wpr1,
	ramcfg_sram1wpr2,
	ramcfg_sram1parkeyr = 0x9U,
	ramcfg_sram1erkeyr,
	ramcfg_sram2cr = 0x10U,
	ramcfg_sram2ier,
	ramcfg_sram2isr,
	ramcfg_sram2pear = 0x14U,
	ramcfg_sram2icr,
	ramcfg_sram2wpr1,
	ramcfg_sram2wpr2,
	ramcfg_sram2parkeyr = 0x19U,
	ramcfg_sram2erkeyr,
	ramcfg_sram3cr = 0x20U,
	ramcfg_sram3ier,
	ramcfg_sram3isr,
	ramcfg_sram3pear = 0x24U,
	ramcfg_sram3icr,
	ramcfg_sram3wpr1,
	ramcfg_sram3wpr2,
	ramcfg_sram3parkeyr = 0x29U,
	ramcfg_sram3erkeyr,
};

enum {
	gpdma_seccfgr = 0x0,
	gpdma_privcfgr,
	gpdma_rcfglockr,
	gpdma_misr,
	gpdma_smisr,
	gpdma_cxlbar = 0x14,
};

enum {
	dbgmcu_idcode = 0x0U,
	dbgmcu_cr,
	dbgmcu_apb1lfzr,
	dbgmcu_apb1hfzr,
	dbgmcu_apb2fzr,
	dbgmcu_apb3fzr,
	dbgmcu_ahb1fzr = 0x8U,
	dbgmcu_sr = 0x3fU,
	dbgmcu_dgb_auth_host,
	dbgmcu_dgb_auth_device,
	dbgmcu_pidr4 = 0x3f4U,
	dbgmcu_pidr0 = 0x3f8U,
	dbgmcu_pidr1,
	dbgmcu_pidr2,
	dbgmcu_pidr3,
	dbgmcu_cidr0,
	dbgmcu_cidr1,
	dbgmcu_cidr2,
	dbgmcu_cidr3,
};

#endif /* _PH_STM32U3_REGS_H_ */
