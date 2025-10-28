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

#ifndef _HAL_STM32_H_
#define _HAL_STM32_H_

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


/* platformctl syscall */
extern int hal_platformctl(void *);


/* Sets peripheral's bus clock */
extern int _stm32_rccSetDevClock(unsigned int dev, u32 status, u32 lpStatus);


extern int _stm32_rccGetDevClock(unsigned int dev, u32 *status, u32 *lpStatus);


/* Sets independent peripheral clock configuration */
extern int _stm32_rccSetIPClk(unsigned int ipclk, unsigned int setting);


extern int _stm32_rccGetIPClk(unsigned int ipclk, unsigned int *setting_out);


/* Get frequency of CPU clock in Hz */
extern u32 _stm32_rccGetCPUClock(void);


/* Get frequency of PER (common peripheral) clock in Hz */
extern u32 _stm32_rccGetPerClock(void);


extern void _stm32_rccClearResetFlags(void);


/* If `stop` != 0, selected timer will be stopped when CPU is halted in debug. */
extern int _stm32_dbgmcuStopTimerInDebug(unsigned int dev, u32 stop);


extern int _stm32_gpioConfig(unsigned int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd);


extern int _stm32_gpioSet(unsigned int d, u8 pin, u8 val);


extern int _stm32_gpioSetPort(unsigned int d, u16 val);


extern int _stm32_gpioGet(unsigned int d, u8 pin, u8 *val);


extern int _stm32_gpioGetPort(unsigned int d, u16 *val);


extern int _stm32_gpioSetPrivilege(unsigned int d, u32 val);


extern int _stm32_gpioGetPrivilege(unsigned int d, u32 *val);


extern void _stm32_rtcUnlockRegs(void);


extern void _stm32_rtcLockRegs(void);


extern int _stm32_extiMaskInterrupt(u32 line, u8 state);


extern int _stm32_extiMaskEvent(u32 line, u8 state);


/* state: 1 - enable, 0 - disable
 * edge: 1 - rising, 0 - falling
 */
extern int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge);


extern int _stm32_syscfgExtiLineConfig(u8 port, u8 pin);


extern int _stm32_extiSoftInterrupt(u32 line);


extern u32 _stm32_extiGetPending(void);


extern int _stm32_extiClearPending(u32 line);


extern void _stm32_wdgReload(void);


extern int _stm32_rifsc_risup_change(unsigned int index, int secure, int privileged, int lock);


extern int _stm32_rifsc_rimc_change(unsigned int index, int secure, int privileged, int cid);


extern int _stm32_bsec_otp_checkFuseValid(unsigned int addr);


extern int _stm32_bsec_otp_read(unsigned int addr, u32 *val);


extern int _stm32_bsec_otp_write(unsigned int addr, u32 val);


extern void _stm32_bsec_init(void);


extern int _stm32_dmaSetPermissions(int dev, unsigned int channel, int secure, int privileged, int lock);


extern int _stm32_risaf_configRegion(unsigned int risaf, u8 region, u32 start, u32 end, u8 privCIDMask, u8 readCIDMask, u8 writeCIDMask, int secure, int enable);


extern int _stm32_risaf_init(void);


extern void _stm32_init(void);


#endif
