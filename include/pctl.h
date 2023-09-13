/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System platform control
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_PCTL_H_
#define _PHOENIX_PCTL_H_

#include "arch.h"

/* Include platform dependent types */
#ifdef __PHOENIX_ARCH_PCTL
#include __PHOENIX_ARCH_PCTL
#endif


#endif
