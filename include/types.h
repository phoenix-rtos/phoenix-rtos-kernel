/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Arch-independent types
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_TYPES_H_
#define _PHOENIX_TYPES_H_


#if defined(__i386__) || defined(__x86_64__)
#include "arch/ia32/types.h"
#elif defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
#include "arch/armv7m/types.h"
#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_8A__) || defined(__ARM_ARCH_7__)
#include "arch/armv7a/types.h"
#elif defined(__ARM_ARCH_7R__)
#include "arch/armv7r/types.h"
#elif defined(__ARM_ARCH_4T__) || defined(__ARM_ARCH_5TE__) /* not currently supported, map to 7M for libgcc to compile */
#include "arch/armv7m/types.h"
#elif defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
#include "arch/armv8m/types.h"
#elif defined(__ARM_ARCH_8R__)
#include "arch/armv8r/types.h"
#elif defined(__aarch64__)
#include "arch/aarch64/types.h"
#elif defined(__riscv) && (__riscv_xlen == 64)
#include "arch/riscv64/types.h"
#elif defined(__sparc__)
#include "arch/sparcv8leon/types.h"
#else
#error "unsupported architecture"
#endif

#include "posix-types.h"


typedef __s64 off64_t;
typedef off64_t off_t;

/* Object identifier - contains server port and object id */
typedef struct _oid_t {
	__u32 port;
	id_t id;
} oid_t;


typedef int handle_t;

typedef long long time_t;


#endif
