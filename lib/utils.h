/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Miscellaneous utilities
 *
 * Copyright 2012, 2014, 2016 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_LIB_UTILS_H_
#define _PH_LIB_UTILS_H_

#include "include/page.h"

/* TODO: migrate to C11 stdatomic - check the performance impact of seq_cnt (mandatory by MISRA) */
/* casts needed as return type is not always the same as *ptr (why? GCC bug?) */
#define lib_atomicIncrement(ptr) ((typeof(*(ptr)))__atomic_add_fetch((ptr), 1, __ATOMIC_RELAXED))
#define lib_atomicDecrement(ptr) ((typeof(*(ptr)))__atomic_sub_fetch((ptr), 1, __ATOMIC_ACQ_REL))

#define max(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_a > _b ? _a : _b; \
})

#define min(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_a > _b ? _b : _a; \
})

#define swap(a, b) ({ \
	__typeof__(a) tmp = (a); \
	(a) = (b); \
	(b) = (tmp); \
})

/* parasoft-suppress-next-line MISRAC2012-RULE_20_7-a "__builtin_offsetof is built-in function and handles it arguments safely" */
#define offsetof(st, m) __builtin_offsetof(st, m)


/* parasoft-begin-suppress MISRAC2012-RULE_10_3-b "__atomic intrinsics are type-generic and internally are mostly implemented as taking unsigned types, thereby confusing the rule checker" */

static inline void atomic_store_int(int *ptr, int val, int memmodel)
{
	__atomic_store_n(ptr, val, memmodel);
}


static inline void atomic_store_uint(unsigned int *ptr, unsigned int val, int memmodel)
{
	__atomic_store_n(ptr, val, memmodel);
}


static inline int atomic_load_int(const int *ptr, int memmodel)
{
	return __atomic_load_n(ptr, memmodel);
}


static inline unsigned int atomic_load_uint(const unsigned int *ptr, int memmodel)
{
	return __atomic_load_n(ptr, memmodel);
}


/* Round size up to page size. Return value is guaranteed to be >= x. */
static inline size_t round_page(size_t x)
{
	if (__builtin_add_overflow(x, _PAGE_SIZE - 1U, &x)) {
		/*
		 * If overflow happens, return maximum value representable in size_t.
		 * Caller should detect that this value is invalid and take appropriate action.
		 */
		return (size_t)-1;
	}

	return x & ~(_PAGE_SIZE - 1U);
}


/* Return number of pages needed to contain given size. */
static inline size_t num_pages(size_t x)
{
	return (x / _PAGE_SIZE) + (((x % _PAGE_SIZE) != 0U) ? 1U : 0U);
}

/* parasoft-end-suppress MISRAC2012-RULE_10_3-b */

#endif
