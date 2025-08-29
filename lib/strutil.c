/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * String helper routines
 *
 * Copyright 2018, 2023 Phoenix Systems
 * Author: Jan Sikorski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "vm/kmalloc.h"
#include "lib/strutil.h"

/* MISRA Rule 10.3: int c changed to char c to adhere */
static char *lib_strrchr(char *s, char c)
{
	char *p = NULL;
	char *i;

	for (i = s; *i != '\0'; ++i) {
		/* MISRA Rule 10.4: As this function compares to char, the input
		 * cast can be safely performed
		 */
		if (*i == c) {
			p = i;
		}
	}

	return p;
}


char *lib_strdup(const char *str)
{
	size_t len = hal_strlen(str) + 1U;
	char *ptr = vm_kmalloc(len);

	if (ptr != NULL) {
		hal_memcpy(ptr, str, len);
	}

	return ptr;
}


void lib_splitname(char *path, char **base, const char **dir)
{
	char *slash;

	slash = lib_strrchr(path, '/');

	if (slash == NULL) {
		*dir = ".";
		*base = path;
	}
	else if (slash == path) {
		*base = path + 1;
		*dir = "/";
	}
	else {
		*dir = path;
		*base = slash + 1;
		*slash = '\0';
	}
}
