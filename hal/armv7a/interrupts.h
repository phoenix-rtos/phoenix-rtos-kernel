/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling
 *
 * Copyright 2016, 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_INTERRUPTS_H_
#define _HAL_INTERRUPTS_H_

#ifndef __ASSEMBLY__

#include "cpu.h"
#include "pmap.h"


#define _intr_add(list, t) \
	do { \
		if (t == NULL) \
			break; \
		if (*list == NULL) { \
			t->next = t; \
			t->prev = t; \
			(*list) = t; \
			break; \
		} \
		t->prev = (*list)->prev; \
		(*list)->prev->next = t; \
		t->next = (*list); \
		(*list)->prev = t; \
	} while (0)


#define _intr_remove(list, t) \
	do { \
		if (t == NULL) \
			break; \
		if ((t->next == t) && (t->prev == t)) \
			(*list) = NULL; \
		else { \
			t->prev->next = t->next; \
			t->next->prev = t->prev; \
			if (t == (*list)) \
				(*list) = t->next; \
		} \
		t->next = NULL; \
		t->prev = NULL; \
	} while (0)


typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	int (*f)(unsigned int, cpu_context_t *, void *);
	void *data;
} intr_handler_t;


/* Function installs new handler for interrupt given by n */
extern int hal_interruptsSetHandler(intr_handler_t *h);


extern int hal_interruptsDeleteHandler(intr_handler_t *h);


extern char *hal_interruptsFeatures(char *features, unsigned int len);


/* Function initializes interrupt handling */
extern void _hal_interruptsInit(void);


#endif

#endif
