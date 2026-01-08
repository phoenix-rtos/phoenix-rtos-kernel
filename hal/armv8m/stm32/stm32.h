/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32 basic peripherals control functions
 *
 * Copyright 2017, 2019-2020, 2025 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_STM32_H_
#define _PH_HAL_STM32_H_

#include "hal/types.h"
#include "hal/pmap.h"

#ifdef __CPU_STM32N6
#include "include/arch/armv8m/stm32/n6/stm32n6.h"
#endif


enum gpio_modes {
	gpio_mode_gpi = 0,
	gpio_mode_gpo = 1,
	gpio_mode_af = 2,
	gpio_mode_analog = 3,
};


enum gpio_otypes {
	gpio_otype_pp = 0,
	gpio_otype_od = 1,
};


enum gpio_ospeeds {
	gpio_ospeed_low = 0,
	gpio_ospeed_med = 1,
	gpio_ospeed_hi = 2,
	gpio_ospeed_vhi = 3,
};


enum gpio_pupds {
	gpio_pupd_nopull = 0,
	gpio_pupd_pullup = 1,
	gpio_pupd_pulldn = 2,
};


/* Sets peripheral's bus clock */
int _stm32_rccSetDevClock(int dev, u32 status, u32 lpStatus);


int _stm32_rccGetDevClock(int dev, u32 *status, u32 *lpStatus);


/* Sets independent peripheral clock configuration */
int _stm32_rccSetIPClk(unsigned int ipclk, unsigned int setting);


int _stm32_rccGetIPClk(unsigned int ipclk, unsigned int *setting_out);


/* Get frequency of CPU clock in Hz */
u32 _stm32_rccGetCPUClock(void);


/* Get frequency of PER (common peripheral) clock in Hz */
u32 _stm32_rccGetPerClock(void);


void _stm32_rccClearResetFlags(void);


/* If `stop` != 0, selected timer will be stopped when CPU is halted in debug. */
int _stm32_dbgmcuStopTimerInDebug(int dev, u32 stop);


int _stm32_gpioConfig(int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd);


int _stm32_gpioSet(int d, u8 pin, u8 val);


int _stm32_gpioSetPort(int d, u16 val);


int _stm32_gpioGet(int d, u8 pin, u8 *val);


int _stm32_gpioGetPort(int d, u32 *val);


int _stm32_gpioSetPrivilege(int d, u32 val);


int _stm32_gpioGetPrivilege(int d, u32 *val);


void _stm32_rtcUnlockRegs(void);


void _stm32_rtcLockRegs(void);


int _stm32_extiMaskInterrupt(u32 line, u8 state);


int _stm32_extiMaskEvent(u32 line, u8 state);


/* state: 1 - enable, 0 - disable
 * edge: 1 - rising, 0 - falling
 */
int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge);


int _stm32_extiSoftInterrupt(u32 line);


void _stm32_wdgReload(void);


int _stm32_rifsc_risup_change(int index, int secure, int privileged, int lock);


int _stm32_rifsc_rimc_change(int index, int secure, int privileged, int cid);


int _stm32_bsec_otp_checkFuseValid(unsigned int fuse);


int _stm32_bsec_otp_read(unsigned int fuse, u32 *val);


int _stm32_bsec_otp_write(unsigned int fuse, u32 val);


void _stm32_bsec_init(void);


int _stm32_dmaSetPermissions(int dev, unsigned int channel, int secure, int privileged, int lock);


int _stm32_dmaSetLinkBaseAddr(int dev, unsigned int channel, unsigned int addr);

int _stm32_AXICacheCmd(void *addr, unsigned int sz, int cmdtype);

int _stm32_setAXICacheEnable(unsigned int enable);

int _stm32_getAXICacheEnable(void);

int _stm32_risaf_configRegion(int risaf, u8 region, u32 start, u32 end, u8 privCIDMask, u8 readCIDMask, u8 writeCIDMask, int secure, int enable);


int _stm32_risaf_init(void);


void _stm32_init(void);


#endif
