/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Page shift definition - SPARC
 *
 * Copyright 2026 Phoenix Systems
 * Author: Michal Lach
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_ARCH_LEON3_PAGE_H_
#define _PH_ARCH_LEON3_PAGE_H_

#ifdef NOMMU
#define _PAGE_SHIFT 9U
#else
#define _PAGE_SHIFT 12U
#endif

#endif
