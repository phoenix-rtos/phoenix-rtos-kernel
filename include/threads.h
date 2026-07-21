/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Mutexes/Condvars
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_THREADS_H_
#define _PH_THREADS_H_


/* Mutex attributes */


#define PH_LOCK_NORMAL     0U
#define PH_LOCK_RECURSIVE  1U
#define PH_LOCK_ERRORCHECK 2U

#define PH_LOCK_PROTO_INHERIT     0U
#define PH_LOCK_PROTO_NOINHERIT   1U
#define PH_LOCK_PROTO_PRIOCEILING 2U

#define PH_LOCK_STALLED 0U
#define PH_LOCK_ROBUST  1U


struct lockAttr {
	unsigned char type;
	unsigned char protocol;
	unsigned char robust;
	unsigned char prioceiling;
};


/* Condvar attributes */


#define PH_COND_NORMAL   0
#define PH_COND_UNLOCKED 1 /* Cond without lock - can be used for waiting for IRQ signal or sync on atomics */


struct condAttr {
	int clock;
	int type;
};


#endif
