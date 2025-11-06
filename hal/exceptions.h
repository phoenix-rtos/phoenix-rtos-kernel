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

#ifndef _PH_HAL_EXCEPTIONS_H_
#define _PH_HAL_EXCEPTIONS_H_

#include "vm/types.h"

#include <arch/exceptions.h>


typedef void (*excHandlerFn_t)(unsigned int n, exc_context_t *ctx);


vm_prot_t hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx);


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx);


ptr_t hal_exceptionsPC(exc_context_t *ctx);


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n);


int hal_exceptionsSetHandler(unsigned int n, excHandlerFn_t handler);


void _hal_exceptionsInit(void);

#endif
