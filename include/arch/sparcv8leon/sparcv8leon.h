/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * SPARC V8 LEON basic peripherals control functions
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_SPARCV8LEON_H_
#define _PHOENIX_ARCH_SPARCV8LEON_H_


#if defined(__CPU_GR716)
#include "gr716/gr716.h"
#elif defined(__CPU_GR712RC)
#include "gr712rc/gr712rc.h"
#elif defined(__CPU_GENERIC)
#include "generic/generic.h"
#else
#error "Unsupported TARGET"
#endif


#endif
