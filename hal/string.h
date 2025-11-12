/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL basic routines
 *
 * Copyright 2017 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_STRING_H_
#define _PH_HAL_STRING_H_

#include "hal/types.h"


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly code" */
void hal_memcpy(void *dst, const void *src, size_t l);


int hal_memcmp(const void *ptr1, const void *ptr2, size_t num);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly code" */
void hal_memset(void *dst, int v, size_t l);


size_t hal_strlen(const char *s);


int hal_strcmp(const char *s1, const char *s2);


int hal_strncmp(const char *s1, const char *s2, size_t n);


char *hal_strcpy(char *dest, const char *src);


char *hal_strncpy(char *dest, const char *src, size_t n);


/* TODO: meaningful naming of signature and implementation (hal_i2s -> hal_ul2str(?), i -> val, b -> base, l -> length, etc.) */
unsigned long hal_i2s(const char *prefix, char *s, unsigned long i, u8 b, u8 zero);


#endif
