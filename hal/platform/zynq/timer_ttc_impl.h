/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver using TTC peripheral header
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _TIMER_TTC_IMPL_H_
#define _TIMER_TTC_IMPL_H_

#include "hal/types.h"

/* Functions below must be implemented in platform-specific code. */


volatile u32 *_zynq_ttc_getAddress(void);


void _zynq_ttc_performReset(void);


#endif /* _TIMER_TTC_IMPL_H_ */
