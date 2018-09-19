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

#ifndef _PROC_USERINTR_H_
#define _PROC_USERINTR_H_

#include HAL

extern int userintr_setHandler(unsigned int n, int (*f)(unsigned int, void *), void *arg, unsigned int cond, unsigned int *h);


extern int userintr_dispatch(intr_handler_t *h);


extern intr_handler_t *userintr_active(void);


extern void _userintr_init(void);

#endif
