/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon3-gr712rc
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_GR716_H_
#define _HAL_GR716_H_


#ifndef __ASSEMBLY__


#include "hal/types.h"
#include <board_config.h>

#include "hal/sparcv8leon3/gaisler/ambapp.h"
#include "hal/sparcv8leon3/gaisler/gaisler.h"

#include "include/arch/sparcv8leon3/gr712rc/gr712rc.h"


/* Timer registers */

#define GPT_SCALER  0 /* Scaler value register        : 0x00 */
#define GPT_SRELOAD 1 /* Scaler reload value register : 0x04 */
#define GPT_CONFIG  2 /* Configuration register       : 0x08 */

#define GPT_TCNTVAL1 4 /* Timer 1 counter value reg    : 0x10 */
#define GPT_TRLDVAL1 5 /* Timer 1 reload value reg     : 0x14 */
#define GPT_TCTRL1   6 /* Timer 1 control register     : 0x18 */

#define GPT_TCNTVAL2 8  /* Timer 2 counter value reg    : 0x20 */
#define GPT_TRLDVAL2 9  /* Timer 2 reload value reg     : 0x24 */
#define GPT_TCTRL2   10 /* Timer 2 control register     : 0x28 */

#define GPT_TCNTVAL3 12 /* Timer 3 counter value reg    : 0x30 */
#define GPT_TRLDVAL3 13 /* Timer 3 reload value reg     : 0x34 */
#define GPT_TCTRL3   14 /* Timer 3 control register     : 0x38 */

#define GPT_TCNTVAL4 16 /* Timer 4 counter value reg    : 0x40 */
#define GPT_TRLDVAL4 17 /* Timer 4 reload value reg     : 0x44 */
#define GPT_TCTRL4   18 /* Timer 4 control register     : 0x48 */


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn);


void _gr712rc_cguClkEnable(u32 device);


void _gr712rc_cguClkDisable(u32 device);


int _gr712rc_cguClkStatus(u32 device);


int hal_platformctl(void *ptr);


void _hal_platformInit(void);


#endif /* __ASSEMBLY__ */


#endif
