/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Library routines
 *
 * Copyright 2012, 2014, 2016 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_LIB_LIB_H_
#define _PH_LIB_LIB_H_

#include "hal/hal.h"
#include "printf.h"
#include "bsearch.h"
#include "rand.h"
#include "rb.h"
#include "list.h"
#include "assert.h"
#include "strutil.h"
#include "idtree.h"


#define lib_atomicIncrement(ptr) __atomic_add_fetch((ptr), 1, __ATOMIC_RELAXED)


#define lib_atomicDecrement(ptr) __atomic_add_fetch((ptr), -1, __ATOMIC_ACQ_REL)


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


#include "cbuffer.h"


#define swap(a, b) ({ \
	__typeof__(a) tmp = (a); \
	(a) = (b); \
	(b) = (tmp); \
})


#define round_page(x) (((x) + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U))

/* parasoft-suppress-next-line MISRAC2012-RULE_20_7-a "__builtin_offsetof is built-in function and handles it arguments safely" */
#define offsetof(st, m) __builtin_offsetof(st, m)


#endif
