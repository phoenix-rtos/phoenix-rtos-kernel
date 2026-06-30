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
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _PH_LIB_BSEARCH_H_
#define _PH_LIB_BSEARCH_H_


void *lib_bsearch(void *key, void *base, size_t nmemb, size_t size, int (*compar)(void *n1, void *n2));

#endif
