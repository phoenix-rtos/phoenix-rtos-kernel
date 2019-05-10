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

enum { gpio1 = 0, gpio2, gpio3, gpio4, gpio5 };


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


/* CPU modes */
enum { clk_mode_run = 0, clk_mode_wait, clk_mode_stop };


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


extern int _imxrt_setIOmux(int mux, char sion, char mode);


extern int _imxrt_setIOpad(int pad, char hys, char pus, char pue, char pke, char ode, char speed, char dse, char sre);


extern void _imxrt_invokePendSV(void);


extern void _imxrt_invokeSysTick(void);


extern unsigned int _imxrt_cpuid(void);


extern void _imxrt_wdgReload(void);


extern void _imxrt_init(void);


#endif
