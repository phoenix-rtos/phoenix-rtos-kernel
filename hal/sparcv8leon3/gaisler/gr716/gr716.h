/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon3-gr716
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_GR716_H_
#define _HAL_GR716_H_


#ifndef __ASSEMBLY__


#include <arch/types.h>
#include <board_config.h>

#include "hal/sparcv8leon3/gaisler/ambapp.h"
#include "hal/sparcv8leon3/gaisler/gaisler.h"


#define TIMER_IRQ 9

/* Timer registers */

#define GPT_SCALER   0  /* Scaler value register        : 0x00 */
#define GPT_SRELOAD  1  /* Scaler reload value register : 0x04 */
#define GPT_CONFIG   2  /* Configuration register       : 0x08 */
#define GPT_LATCHCFG 3  /* Latch configuration register : 0x0C */
#define GPT_TCNTVAL1 4  /* Timer 1 counter value reg    : 0x10 */
#define GPT_TRLDVAL1 5  /* Timer 1 reload value reg     : 0x14 */
#define GPT_TCTRL1   6  /* Timer 1 control register     : 0x18 */
#define GPT_TLATCH1  7  /* Timer 1 latch register       : 0x1C */
#define GPT_TCNTVAL2 8  /* Timer 2 counter value reg    : 0x20 */
#define GPT_TRLDVAL2 9  /* Timer 2 reload value reg     : 0x24 */
#define GPT_TCTRL2   10 /* Timer 2 control register     : 0x28 */
#define GPT_TLATCH2  11 /* Timer 2 latch register       : 0x2C */
#define GPT_TCNTVAL3 12 /* Timer 3 counter value reg    : 0x30 */
#define GPT_TRLDVAL3 13 /* Timer 3 reload value reg     : 0x34 */
#define GPT_TCTRL3   14 /* Timer 3 control register     : 0x38 */
#define GPT_TLATCH3  15 /* Timer 3 latch register       : 0x3C */
#define GPT_TCNTVAL4 16 /* Timer 4 counter value reg    : 0x40 */
#define GPT_TRLDVAL4 17 /* Timer 4 reload value reg     : 0x44 */
#define GPT_TCTRL4   18 /* Timer 4 control register     : 0x48 */
#define GPT_TLATCH4  19 /* Timer 4 latch register       : 0x4C */
#define GPT_TCNTVAL5 20 /* Timer 5 counter value reg    : 0x50 */
#define GPT_TRLDVAL5 21 /* Timer 5 reload value reg     : 0x54 */
#define GPT_TCTRL5   22 /* Timer 5 control register     : 0x58 */
#define GPT_TLATCH5  23 /* Timer 5 latch register       : 0x5C */
#define GPT_TCNTVAL6 24 /* Timer 6 counter value reg    : 0x60 */
#define GPT_TRLDVAL6 25 /* Timer 6 reload value reg     : 0x64 */
#define GPT_TCTRL6   26 /* Timer 6 control register     : 0x68 */
#define GPT_TLATCH6  27 /* Timer 6 latch register       : 0x6C */
#define GPT_TCNTVAL7 28 /* Timer 7 counter value reg    : 0x70 */
#define GPT_TRLDVAL7 29 /* Timer 7 reload value reg     : 0x74 */
#define GPT_TCTRL7   30 /* Timer 7 control register     : 0x78 */
#define GPT_TLATCH7  31 /* Timer 7 latch register       : 0x7C */


extern int _gr716_getIomuxCfg(u8 pin, u8 *opt, u8 *pullup, u8 *pulldn);


extern void _gr716_cguClkEnable(u32 cgu, u32 device);


extern void _gr716_cguClkDisable(u32 cgu, u32 device);


extern int _gr716_cguClkStatus(u32 cgu, u32 device);


extern int hal_platformctl(void *ptr);


extern void _hal_platformInit(void);


#endif /* __ASSEMBLY__ */


#endif
