/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (ARMv7r)
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_ARMV7R_H_
#define _PH_HAL_PMAP_ARMV7R_H_

#include "hal/types.h"
#include "hal/page.h"

#define PGHD_PRESENT    0x01U
#define PGHD_USER       0x04U
#define PGHD_WRITE      0x02U
#define PGHD_EXEC       0x00U
#define PGHD_DEV        0x00U
#define PGHD_NOT_CACHED 0x00U
#define PGHD_READ       0x00U

#ifndef __ASSEMBLY__

typedef struct _pmap_t {
	void *start;
	void *end;
	u32 regions;
} pmap_t;

#endif

#endif
