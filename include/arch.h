/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System architecture dependent definitions
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_H_
#define _PHOENIX_ARCH_H_


#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_8A__) || defined(__ARM_ARCH_7__)
#include "arch/armv7a/arch.h"
#elif defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
#include "arch/armv7m/arch.h"
#elif defined(__ARM_ARCH_4T__) || defined(__ARM_ARCH_5TE__) /* Not currently supported - map to armv7m */
#include "arch/armv7m/arch.h"
#elif defined(__i386__) || defined(__x86_64__)
#include "arch/ia32/arch.h"
#elif defined(__riscv) && (__riscv_xlen == 64)
#include "arch/riscv64/arch.h"
#else
#error "unsupported architecture"
#endif


#endif
