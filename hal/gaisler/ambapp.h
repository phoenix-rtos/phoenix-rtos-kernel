/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * AMBA Plug'n'Play
 *
 * Copyright 2023, 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_AMBAPP_H_
#define _PH_HAL_AMBAPP_H_


#include "include/gaisler/ambapp.h"


int ambapp_findMaster(ambapp_dev_t *dev, unsigned int *instance);


int ambapp_findSlave(ambapp_dev_t *dev, unsigned int *instance);


void ambapp_init(void);


#endif
