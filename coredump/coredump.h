/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Coredump support
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include "proc/process.h"


extern int coredump_enqueue(process_t *process);


extern void _coredump_start(void);


#endif /* _COREDUMP_H_ */
