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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _GAISLER_H_
#define _GAISLER_H_

#include <config.h>
#include "hal/types.h"


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn);


void gaisler_cpuHalt(void);


void hal_cpuStartCores(void);


#endif
