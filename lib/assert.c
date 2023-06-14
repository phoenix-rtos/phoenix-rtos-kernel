
/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Debug assert
 *
 * Copyright 2023 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdarg.h>
#include "assert.h"
#include "printf.h"
#include "../log/log.h"
#include "../hal/hal.h"


void lib_assertPanic(const char *func, int line, const char *fmt, ...)
{
	va_list ap;

	log_disable();
	hal_cpuDisableInterrupts();
	lib_printf("kernel (%s:%d): ", func, line);
	va_start(ap, fmt);
	lib_vprintf(fmt, ap);
	va_end(ap);
	lib_putch('\n');
	for (;;) {
		hal_cpuHalt();
	}
}
