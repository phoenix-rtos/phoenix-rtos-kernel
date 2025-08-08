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


extern void lib_listAdd(void **list, void *t, size_t noff, size_t poff);


extern void lib_listRemove(void **list, void *t, size_t noff, size_t poff);


extern int lib_listBelongs(void **list, void *t, size_t noff, size_t poff);


#define LIST_ADD_EX(list, t, next, prev) \
	lib_listAdd((void **)(list), (void *)(t), (size_t) & (((typeof(t))0)->next), (size_t) & (((typeof(t))0)->prev))


#define LIST_ADD(list, t) LIST_ADD_EX((list), (t), next, prev)


#define LIST_REMOVE_EX(list, t, next, prev) \
	lib_listRemove((void **)(list), (void *)(t), (size_t) & (((typeof(t))0)->next), (size_t) & (((typeof(t))0)->prev))


#define LIST_REMOVE(list, t) LIST_REMOVE_EX((list), (t), next, prev)


#define LIST_BELONGS_EX(list, t, next, prev) \
	lib_listBelongs((void **)(list), (void *)(t), (size_t) & (((typeof(t))0)->next), (size_t) & (((typeof(t))0)->prev))


#define LIST_BELONGS(list, t) LIST_BELONGS_EX((list), (t), next, prev)

#endif
