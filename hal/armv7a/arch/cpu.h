/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2018 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV7A_CPU_H_
#define _HAL_ARMV7A_CPU_H_

#include "types.h"

#define SIZE_PAGE       0x1000
#define SIZE_PDIR       0x4000
#define SIZE_CACHE_LINE 64

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (8 * 512)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8 * SIZE_PAGE)
#endif

#define USR_MODE    0x10
#define FIQ_MODE    0x11
#define IRQ_MODE    0x12
#define SVC_MODE    0x13 /* reset mode */
#define ABT_MODE    0x17
#define UND_MODE    0x1B
#define SYS_MODE    0x1F
#define MODE_MASK   0x1F
#define NO_ABORT    0x100             /* mask to disable Abort Exception */
#define NO_IRQ      0x80              /* mask to disable IRQ */
#define NO_FIQ      0x40              /* mask to disable FIQ */
#define NO_INT      (NO_IRQ | NO_FIQ) /* mask to disable IRQ and FIQ */
#define THUMB_STATE 0x20


#ifndef __ASSEMBLY__

#define SYSTICK_INTERVAL 1000


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= (sizeof(t) + 3) & ~0x3; \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (void *)(((addr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
		ustack += (sizeof(t) + 3) & ~0x3; \
	} while (0)

typedef struct _cpu_context_t {
	u32 savesp;
	u32 padding;

	/* Floating point coprocessor context */
	u32 fpsr;
	u32 freg[32 * 2];

	u32 psr;

	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;

	u32 fp;
	u32 ip;
	u32 sp;
	u32 lr;

	u32 pc;
} cpu_context_t;


#endif

#endif
