/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * List
 *
 * Copyright 2023 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _HAL_LIST_H_
#define _HAL_LIST_H_


/* list has to be a struct with pointer fields prev and next to another list */


#define HAL_LIST_ADD(list, t) \
	do { \
		if ((t) == NULL) { \
			break; \
		} \
		if (*(list) == NULL) { \
			(t)->next = (t); \
			(t)->prev = (t); \
			(*(list)) = (t); \
			break; \
		} \
		(t)->prev = (*(list))->prev; \
		(*(list))->prev->next = (t); \
		(t)->next = (*(list)); \
		(*(list))->prev = (t); \
	} while (0)


#define HAL_LIST_REMOVE(list, t) \
	do { \
		if ((t) == NULL) { \
			break; \
		} \
		if (((t)->next == (t)) && ((t)->prev == (t))) { \
			(*(list)) = NULL; \
		} \
		else { \
			(t)->prev->next = (t)->next; \
			(t)->next->prev = (t)->prev; \
			if ((t) == (*(list))) { \
				(*(list)) = (t)->next; \
			} \
		} \
		(t)->next = NULL; \
		(t)->prev = NULL; \
	} while (0)


#endif
