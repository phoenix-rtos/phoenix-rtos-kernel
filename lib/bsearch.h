/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Binary search
 *
 * Copyright 2012 Phoenix Systems
 * Author: Paweł Kołodziej
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 *
 */

#ifndef LIB_BSEARCH_H_
#define LIB_BSEARCH_H_


extern void *lib_bsearch(void *key, void *base, size_t nmemb, size_t size, int (*compar)(void *n1, void *n2));

#endif
