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


#define GPIO_PORT_0 0
#define GPIO_PORT_1 1

#define GPIO_DIR_IN  0
#define GPIO_DIR_OUT 1


#define UART0_BASE ((void *)0x80300000)
#define UART1_BASE ((void *)0x80301000)
#define UART2_BASE ((void *)0x80302000)
#define UART3_BASE ((void *)0x80303000)
#define UART4_BASE ((void *)0x80304000)
#define UART5_BASE ((void *)0x80305000)

#define GRGPIO0_BASE ((void *)0x8030C000)
#define GRGPIO1_BASE ((void *)0x8030D000)


extern int _gr716_gpioWritePin(u8 pin, u8 val);


extern int _gr716_gpioReadPin(u8 pin, u8 *val);


extern int _gr716_gpioGetPinDir(u8 pin, u8 *dir);


extern int _gr716_gpioSetPinDir(u8 pin, u8 dir);


extern int _gr716_getIOCfg(u8 pin, u8 *opt, u8 *dir, u8 *pullup, u8 *pulldn);


extern int _gr716_setIOCfg(u8 pin, u8 opt, u8 dir, u8 pullup, u8 pulldn);


extern void _gr716_cguClkEnable(u32 cgu, u32 device);


extern void _gr716_cguClkDisable(u32 cgu, u32 device);


extern u32 _gr716_getSysClk(void);


extern int hal_platformctl(void *ptr);


extern void _hal_platformInit(void);


#endif /* __ASSEMBLY__ */


#endif
