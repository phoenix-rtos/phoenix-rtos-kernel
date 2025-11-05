/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Userspace interrupts handling
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_PROC_USERINTR_H_
#define _PH_PROC_USERINTR_H_

#include "hal/hal.h"
#include "cond.h"
#include "resource.h"


typedef int (*userintrFn_t)(unsigned int n, void *arg);

typedef struct _userintr_t {
	resource_t resource;
	intr_handler_t handler;
	process_t *process;
	userintrFn_t f;
	void *arg;
	cond_t *cond;
} userintr_t;


void userintr_put(userintr_t *ui);


int userintr_setHandler(unsigned int n, userintrFn_t f, void *arg, handle_t c);


userintr_t *userintr_active(void);


void _userintr_init(void);

#endif
