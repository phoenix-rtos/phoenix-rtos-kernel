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

#ifndef _PH_LIB_ASSERT_H_
#define _PH_LIB_ASSERT_H_


void lib_assertPanic(const char *func, int line, const char *fmt, ...);


#define LIB_ASSERT_ALWAYS(condition, fmt, ...) \
	if (!(condition)) { \
		lib_assertPanic(__FUNCTION__, __LINE__, (fmt), ##__VA_ARGS__); \
	}

#define LIB_STATIC_ASSERT_SAME_TYPE(t1, t2) _Static_assert(__builtin_types_compatible_p(typeof(t1), typeof(t2)), "type mismatch")

#ifndef NDEBUG

#define LIB_ASSERT(condition, fmt, ...) LIB_ASSERT_ALWAYS((condition), (fmt), ##__VA_ARGS__)

#else

#define LIB_ASSERT(condition, fmt, ...)

#endif

#endif
