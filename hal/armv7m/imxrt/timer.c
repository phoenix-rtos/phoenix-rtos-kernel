/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"

#if defined(CPU_IMXRT105X) || defined(CPU_IMXRT106X)
#include "imxrt10xx.h"
#endif

#ifdef CPU_IMXRT117X
#include "imxrt117x.h"
#endif


void _timer_init(u32 interval)
{
	_imxrt_systickInit(interval);
}
