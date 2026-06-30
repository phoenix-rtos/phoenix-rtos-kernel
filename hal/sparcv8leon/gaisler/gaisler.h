/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Gaisler CPU specific functions
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_GAISLER_H_
#define _PH_GAISLER_H_

#include <config.h>


void hal_cpuStartCores(void);


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn);


__attribute__((noreturn)) void hal_timerWdogReboot(void);


#endif
