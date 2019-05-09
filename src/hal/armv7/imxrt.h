/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * iMXRT basic peripherals control functions
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_IMXRT_H_
#define _HAL_IMXRT_H_

#include "cpu.h"
#include "pmap.h"
#include "spinlock.h"

/* iMXRT peripherals */

/* GPIO pins */
enum { gpio_wakeup = 0, gpio_on, gpio_stby, gpio_test, gpio_por, gpio_onoff, gpio_emc_00, gpio_emc_01, gpio_emc_02,
	gpio_emc_03, gpio_emc_04, gpio_emc_05, gpio_emc_06, gpio_emc_07, gpio_emc_08, gpio_emc_09, gpio_emc_10,
	gpio_emc_11, gpio_emc_12, gpio_emc_13, gpio_emc_14, gpio_emc_15, gpio_emc_16, gpio_emc_17, gpio_emc_18,
	gpio_emc_19, gpio_emc_20, gpio_emc_21, gpio_emc_22, gpio_emc_23, gpio_emc_24, gpio_emc_25, gpio_emc_26,
	gpio_emc_27, gpio_emc_28, gpio_emc_29, gpio_emc_30, gpio_emc_31, gpio_emc_32, gpio_emc_33, gpio_emc_34,
	gpio_emc_35, gpio_emc_36, gpio_emc_37, gpio_emc_38, gpio_emc_39, gpio_emc_40, gpio_emc_41, gpio_ad_b0_00,
	gpio_ad_b0_01, gpio_ad_b0_02, gpio_ad_b0_03, gpio_ad_b0_04, gpio_ad_b0_05, gpio_ad_b0_06, gpio_ad_b0_07,
	gpio_ad_b0_08, gpio_ad_b0_09, gpio_ad_b0_10, gpio_ad_b0_11, gpio_ad_b0_12, gpio_ad_b0_13, gpio_ad_b0_14,
	gpio_ad_b0_15, gpio_ad_b1_00, gpio_ad_b1_01, gpio_ad_b1_02, gpio_ad_b1_03, gpio_ad_b1_04, gpio_ad_b1_05,
	gpio_ad_b1_06, gpio_ad_b1_07, gpio_ad_b1_08, gpio_ad_b1_09, gpio_ad_b1_10, gpio_ad_b1_11, gpio_ad_b1_12,
	gpio_ad_b1_13, gpio_ad_b1_14, gpio_ad_b1_15, gpio_b0_00, gpio_b0_01, gpio_b0_02, gpio_b0_03, gpio_b0_04,
	gpio_b0_05, gpio_b0_06, gpio_b0_07, gpio_b0_08, gpio_b0_09, gpio_b0_10, gpio_b0_11, gpio_b0_12, gpio_b0_13,
	gpio_b0_14, gpio_b0_15, gpio_b1_00, gpio_b1_01, gpio_b1_02, gpio_b1_03, gpio_b1_04, gpio_b1_05, gpio_b1_06,
	gpio_b1_07, gpio_b1_08, gpio_b1_09, gpio_b1_10, gpio_b1_11, gpio_b1_12, gpio_b1_13, gpio_b1_14, gpio_b1_15,
	gpio_sd_b0_00, gpio_sd_b0_01, gpio_sd_b0_02, gpio_sd_b0_03, gpio_sd_b0_04, gpio_sd_b0_05, gpio_sd_b1_00,
	gpio_sd_b1_01, gpio_sd_b1_02, gpio_sd_b1_03, gpio_sd_b1_04, gpio_sd_b1_05, gpio_sd_b1_06, gpio_sd_b1_07,
	gpio_sd_b1_08, gpio_sd_b1_09, gpio_sd_b1_10, gpio_sd_b1_11 };


/* Clock and PLL management */
enum { clk_cpu = 0, clk_ahb, clk_semc, clk_ipg, clk_osc, clk_rtc, clk_armpll, clk_usb1pll, clk_usb1pfd0,
	clk_usb1pfd1, clk_usb1pfd2, clk_usb1pfd3, clk_usb2pll, clk_syspll, clk_syspdf0, clk_syspdf1, clk_syspdf2,
	clk_syspdf3, clk_enetpll0, clk_enetpll1, clk_enetpll2, clk_audiopll, clk_videopll };


enum { clk_pll_arm = 0, clk_pll_sys, clk_pll_usb1, clk_pll_audio, clk_pll_video, clk_pll_enet0, clk_pll_enet1,
	clk_pll_enet2, clk_pll_usb2 };


enum { clk_pfd0 = 0, clk_pfd1, clk_pfd2, clk_pfd3 };


enum { clk_mux_pll3 = 0, clk_mux_periph, clk_mux_semcAlt, clk_mux_semc, clk_mux_prePeriph, clk_mux_trace,
	clk_mux_periphclk2, clk_mux_lpspi, clk_mux_flexspi, clk_mux_usdhc2, clk_mux_usdhc1, clk_mux_sai3, clk_mux_sai2,
	clk_mux_sai1, clk_mux_perclk, clk_mux_flexio2, clk_mux_can, clk_mux_uart, clk_mux_enc, clk_mux_ldbDi1,
	clk_mux_ldbDi0, clk_mux_spdif, clk_mux_flexio1, clk_mux_lpi2c, clk_mux_lcdif1pre, clk_mux_lcdif1, clk_mux_csi };


enum { clk_div_arm = 0, clk_div_periphclk2, clk_div_semc, clk_div_ahb, clk_div_ipg, clk_div_lpspi, clk_div_lcdif1,
	clk_div_flexspi, clk_div_perclk, clk_div_ldbDi1, clk_div_ldbDi0, clk_div_can, clk_div_trace, clk_div_usdhc2,
	clk_div_usdhc1, clk_div_uart, clk_div_flexio2, clk_div_sai3pre, clk_div_sai3, clk_div_flexio2pre, clk_div_sai1pre,
	clk_div_sai1, clk_div_enc, clk_div_encpre, clk_div_sai2pre, clk_div_sai2, clk_div_spdif0pre, clk_div_spdif0,
	clk_div_flexio1pre, clk_div_flexio1, clk_div_lpi2c, clk_div_lcdif1pre, clk_div_csi };


/* Peripherals */
enum { aips_tz1 = 0, aips_tz2, dcp = 5, lpuart3, can1, can1s, can2, can2s, trace, gpt2, gpt2s, lpuart2, gpio2,
	lpspi1, lpspi2, lpspi3, lpspi4, adc_5hc, enet, pit, aoi2, adc1, gpt1 = 26, gpt1s, lpuart4, gpio1, csu, gpio5,
	csi = 33, iomuxcsnvs, lpi2c1, lpi2c2, lpi2c3, ocotp, xbar3, ipmux1, ipmux2, ipmux3, xbar1, xbar2, gpio3, lcd, pxp,
	flexio2, lpuart5, semc, lpuart6, aoi1, lcdpixel, gpio4, ewm, wdog1, flexram, acmp1, acmp2, acmp3, acmp4, ocram, iomuxcsnvsgpr,
	iomuxc = 65, iomuxcgpr, bee, simm7, tscdig, simm, simems, pwm1, pwm2, pwm3, pwm4, enc1, enc2, enc3, enc4,
	rom, flexio1, wdog3, dma, kpp, wdog2, aips_tz4, spdif, simmain, sai1, sai2, sai3, lpuart1, lpuart7, snvshp, snvslp,
	usb0h3, usdhc1, usdhc2, dcdc, ipmux4, flexspi, trng, lpuart8, timer4, aips_tz3, simper, anadig, lpi2c4, timer1, timer2, timer3 };


/* Peripheral clock modes */
enum { clk_state_off = 0, clk_state_run, clk_state_run_wait = 3 };


/* CPU modes */
enum { clk_mode_run = 0, clk_mode_wait, clk_mode_stop };


/* LCD interface */
enum { lcd_RAW8 = 0, lcd_RGB565, lcd_RGB666, lcd_ARGB888, lcd_RGB888 };


enum { lcd_bus8 = 0, lcd_bus16, lcd_bus18, lcd_bus24 };


extern int hal_platformctl(void *ptr);


extern void _imxrt_ccmInitExterlnalClk(void);


extern void _imxrt_ccmDeinitExternalClk(void);


extern void _imxrt_ccmSwitchOsc(int osc);


extern void _imxrt_ccmInitRcOsc24M(void);


extern void _imxrt_ccmDeinitRcOsc24M(void);


extern u32 _imxrt_ccmGetFreq(int name);


extern u32 _imxrt_ccmGetOscFreq(void);


extern void _imxrt_ccmSetOscFreq(u32 freq);


extern void _imxrt_ccmInitArmPll(u32 div);


extern void _imxrt_ccmDeinitArmPll(void);


extern void _imxrt_ccmInitSysPll(u8 div);


extern void _imxrt_ccmDeinitSysPll(void);


extern void _imxrt_ccmInitUsb1Pll(u8 div);


extern void _imxrt_ccmDeinitUsb1Pll(void);


extern void _imxrt_ccmInitUsb2Pll(u8 div);


extern void _imxrt_ccmDeinitUsb2Pll(void);


extern void _imxrt_ccmInitAudioPll(u8 loopdiv, u8 postdiv, u32 num, u32 denom);


extern void _imxrt_ccmDeinitAudioPll(void);


extern void _imxrt_ccmInitVideoPll(u8 loopdiv, u8 postdiv, u32 num, u32 denom);


extern void _imxrt_ccmDeinitVideoPll(void);


extern void _imxrt_ccmInitEnetPll(u8 enclk0, u8 enclk1, u8 enclk2, u8 div0, u8 div1);


extern void _imxrt_ccmDeinitEnetPll(void);


extern u32 _imxrt_ccmGetPllFreq(int pll);


extern void _imxrt_ccmInitSysPfd(int pfd, u8 pfdFrac);


extern void _imxrt_ccmDeinitSysPfd(int pfd);


extern void _imxrt_ccmInitUsb1Pfd(int pfd, u8 pfdFrac);


extern void _imxrt_ccmDeinitUsb1Pfd(int pfd);


extern u32 _imxrt_ccmGetSysPfdFreq(int pfd);


extern u32 _imxrt_ccmGetUsb1PfdFreq(int pfd);


extern u32 _imxrt_ccmGetSysPfdFreq(int pfd);


extern u32 _imxrt_ccmGetUsb1PfdFreq(int pfd);


extern void _imxrt_ccmSetMux(int mux, u32 val);


extern u32 _imxrt_ccmGetMux(int mux);


extern void _imxrt_ccmSetDiv(int div, u32 val);


extern u32 _imxrt_ccmGetDiv(int div);


extern void _imxrt_ccmControlGate(int dev, int state);


extern void _imxrt_ccmSetMode(int mode);


extern void _imxrt_scbSetPriorityGrouping(u32 group);


extern u32 _imxrt_scbGetPriorityGrouping(void);


extern void _imxrt_scbSetPriority(s8 excpn, u32 priority);


extern u32 _imxrt_scbGetPriority(s8 excpn);


extern void _imxrt_nvicSetIRQ(s8 irqn, u8 state);


extern u32 _imxrt_nvicGetPendingIRQ(s8 irqn);


extern void _imxrt_nvicSetPendingIRQ(s8 irqn, u8 state);


extern u32 _imxrt_nvicGetActive(s8 irqn);


extern void _imxrt_nvicSetPriority(s8 irqn, u32 priority);


extern u8 _imxrt_nvicGetPriority(s8 irqn);


extern void _imxrt_nvicSystemReset(void);


extern int _imxrt_systickInit(u32 interval);


extern void _imxrt_systickSet(u8 state);


extern u32 _imxrt_systickGet(void);


extern int _imxrt_gpioConfig(unsigned int d, u8 pin, u8 dir);


extern int _imxrt_gpioSet(unsigned int d, u8 pin, u8 val);


extern int _imxrt_gpioSetPort(unsigned int d, u32 val);


extern int _imxrt_gpioGet(unsigned int d, u8 pin, u8 *val);


extern int _imxrt_gpioGetPort(unsigned int d, u32 *val);


extern void _imxrt_iomuxSetPinMux(int pin, u32 mode, u8 sion);


extern void _imxrt_iomuxSetPinConfig(int pin, u8 hys, u8 pus, u8 pue, u8 pke, u8 ode, u8 speed, u8 dse, u8 sre);


extern void _imxrt_lcdInit(void);


extern void _imxrt_lcdSetTiming(u16 width, u16 height, u32 flags, u8 hsw, u8 hfp, u8 hbp, u8 vsw, u8 vfp, u8 vbp);


extern int _imxrt_lcdSetConfig(int format, int bus);


extern void _imxrt_lcdSetBuffer(void * buffer);


extern void _imxrt_lcdStart(void * buffer);


extern void _imxrt_invokePendSV(void);


extern void _imxrt_invokeSysTick(void);


extern unsigned int _imxrt_cpuid(void);


extern void _imxrt_wdgReload(void);


extern void _imxrt_init(void);


#endif
