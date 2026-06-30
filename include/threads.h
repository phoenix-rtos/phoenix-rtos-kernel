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


#define PH_LOCK_NORMAL     0
#define PH_LOCK_RECURSIVE  1
#define PH_LOCK_ERRORCHECK 2


struct lockAttr {
	int type;
};


/* Condvar attributes */


struct condAttr {
	int clock;
};


#endif
