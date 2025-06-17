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

#ifndef _HAL_PMAP_ARMV7R_H_
#define _HAL_PMAP_ARMV7R_H_

#include "hal/types.h"
#include "hal/page.h"

#define PGHD_PRESENT    0x01u
#define PGHD_USER       0x04u
#define PGHD_WRITE      0x02u
#define PGHD_EXEC       0x00u
#define PGHD_DEV        0x00u
#define PGHD_NOT_CACHED 0x00u
#define PGHD_READ       0x00u

#ifndef __ASSEMBLY__

typedef struct _pmap_t {
	void *start;
	void *end;
	u32 regions;
} pmap_t;

#endif

#endif
