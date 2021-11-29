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

#include "../hal/hal.h"
#include "list.h"


void lib_listAdd(void **list, void *t, size_t noff, size_t poff)
{
	if (t == NULL)
		return;
	if (*list == NULL) {
		*((addr_t *)(t + noff)) = (addr_t)t;
		*((addr_t *)(t + poff)) = (addr_t)t;
		*list = t;
	}
	else {
		*((addr_t *)(t + poff)) = *((addr_t *)(*list + poff));
		*((addr_t *)((void *)*((addr_t *)(*list + poff)) + noff)) = (addr_t)t;
		*((addr_t *)(t + noff)) = *((addr_t *)list);
		*((addr_t *)(*list + poff)) = (addr_t)t;
	}
}


void lib_listRemove(void **list, void *t, size_t noff, size_t poff)
{
	if (t == NULL)
		return;
	if (*((addr_t *)(t + noff)) == (addr_t)t && *((addr_t *)(t + poff)) == (addr_t)t) {
		*list = NULL;
	}
	else {
		*((addr_t *)((void *)(*((addr_t *)(t + poff))) + noff)) = *((addr_t *)(t + noff));
		*((addr_t *)((void *)(*((addr_t *)(t + noff))) + poff)) = *((addr_t *)(t + poff));
		if (t == *list)
			*list = (void *)*((addr_t *)(t + noff));
	}
	*((addr_t *)(t + noff)) = NULL;
	*((addr_t *)(t + poff)) = NULL;
}
