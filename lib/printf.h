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

#ifndef _LIB_PRINTF_H_
#define _LIB_PRINTF_H_

#include <stdarg.h>


extern int lib_vsprintf(char *out, const char *format, va_list args);


extern int lib_printf(const char *fmt, ...);


extern int lib_vprintf(const char *format, va_list ap);


extern void lib_putch(char c);


#endif
