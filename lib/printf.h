/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - printf
 *
 * Copyright 2012, 2014, 2016 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej, Pawel Krezolek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* parasoft-begin-suppress MISRAC2012-RULE_17_1 "stdarg.h required for custom functions that are like printf" */

#ifndef _LIB_PRINTF_H_
#define _LIB_PRINTF_H_

#include <stdarg.h>

int lib_sprintf(char *out, const char *format, ...);


int lib_vsprintf(char *out, const char *format, va_list args);


void lib_printf(const char *format, ...);


int lib_vprintf(const char *format, va_list ap);


void lib_putch(char s);


#endif

/* parasoft-end-suppress MISRAC2012-RULE_17_1 */
