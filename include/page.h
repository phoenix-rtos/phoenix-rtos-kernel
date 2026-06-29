/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Page size definition
 *
 * Copyright 2026 Phoenix Systems
 * Author: Michal Lach
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_PAGE_H_
#define _PH_PAGE_H_

#if defined(__i386__) || defined(__x86_64__)
#include "arch/ia32/page.h"
#elif defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
#include "arch/armv7m/page.h"
#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_8A__) || defined(__ARM_ARCH_7__)
#include "arch/armv7a/page.h"
#elif defined(__ARM_ARCH_7R__)
#include "arch/armv7r/page.h"
#elif defined(__ARM_ARCH_4T__) || defined(__ARM_ARCH_5TE__)
#include "arch/armv7m/page.h"
#elif defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
#include "arch/armv8m/page.h"
#elif defined(__ARM_ARCH_8R__)
#include "arch/armv8r/page.h"
#elif defined(__aarch64__)
#include "arch/aarch64/page.h"
/* parasoft-begin-suppress MISRAC2012-RULE_20_9 "macro defined in riscv architecture,
 * we won't check value of __riscv_xlen if __riscv is not defined"
 */
#elif defined(__riscv) && (__riscv_xlen == 64)
#include "arch/riscv64/page.h"
/*parasoft-end-suppress MISRAC2012-RULE_20_9*/
#elif defined(__sparc__)
#include "arch/sparcv8leon/page.h"
#else
#error "unsupported architecture"
#endif

#ifndef _PAGE_SHIFT
#error "_PAGE_SHIFT is undefined for target architecture"
#endif

#define _PAGE_SIZE (1UL << _PAGE_SHIFT)

#endif /* _PH_PAGE_H_ */
