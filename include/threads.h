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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_THREADS_H_
#define _PHOENIX_THREADS_H_


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
