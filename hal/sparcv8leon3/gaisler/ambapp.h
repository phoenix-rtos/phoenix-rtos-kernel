/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * AMBA Plug'n'Play
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SPARCV8LEON3_AMBAPP_H_
#define _HAL_SPARCV8LEON3_AMBAPP_H_


#include "include/arch/sparcv8leon3.h"


int ambapp_findMaster(ambapp_dev_t *dev, unsigned int *instance);


int ambapp_findSlave(ambapp_dev_t *dev, unsigned int *instance);


#endif
