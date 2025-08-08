/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_EXCEPTIONS_H_
#define _HAL_EXCEPTIONS_H_

#include <arch/exceptions.h>


extern unsigned hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx);


extern void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx);


extern ptr_t hal_exceptionsPC(exc_context_t *ctx);


extern void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n);


extern int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *));


extern void _hal_exceptionsInit(void);

#endif
