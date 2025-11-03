/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - doubly-linked list
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Jan Sikorski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#ifndef _LIB_LIST_H_
#define _LIB_LIST_H_

#include "hal/hal.h"
#include "lib/lib.h"


extern void lib_listAdd(void **list, void *t, size_t noff, size_t poff);


extern void lib_listRemove(void **list, void *t, size_t noff, size_t poff);


extern int lib_listBelongs(void **list, void *t, size_t noff, size_t poff);


#define LIST_ADD_EX(list, t, next, prev) \
	do { \
		LIB_STATIC_ASSERT_SAME_TYPE(*(list), t); \
		lib_listAdd((void **)(list), (void *)(t), offsetof(typeof(*(t)), next), offsetof(typeof(*(t)), prev)); \
	} while (0)


#define LIST_ADD(list, t) LIST_ADD_EX((list), (t), next, prev)


#define LIST_REMOVE_EX(list, t, next, prev) \
	do { \
		LIB_STATIC_ASSERT_SAME_TYPE(*(list), t); \
		lib_listRemove((void **)(list), (void *)(t), offsetof(typeof(*(t)), next), offsetof(typeof(*(t)), prev)); \
	} while (0)


#define LIST_REMOVE(list, t) LIST_REMOVE_EX((list), (t), next, prev)


#define LIST_BELONGS_EX(list, t, next, prev) \
	({ \
		LIB_STATIC_ASSERT_SAME_TYPE(*(list), t); \
		lib_listBelongs((void **)(list), (void *)(t), offsetof(typeof(*(t)), next), offsetof(typeof(*(t)), prev)); \
	})


#define LIST_BELONGS(list, t) LIST_BELONGS_EX((list), (t), next, prev)

#endif
