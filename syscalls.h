/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System calls
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_


#include "arch/cpu.h"


extern const void * const syscalls[];


extern void _syscalls_init(void);


extern cpu_context_t *_syscall_ctx(void);


#endif
