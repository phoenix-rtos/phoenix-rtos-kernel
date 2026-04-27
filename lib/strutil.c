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
#include "proc/threads.h"

static char *lib_strrchr(char *s, char c)
{
	char *p = NULL;
	char *i;

	for (i = s; *i != '\0'; ++i) {
		if (*i == c) {
			p = i;
		}
	}

	return p;
}


#define K_TRY(excctx, oldctx) if (!hal_setexcjmp(&excctx, &oldctx))

#define K_CATCH(excctx, oldctx, op) \
	else \
	{ \
		threads_setexcjmp(oldctx, NULL); \
		op; \
	} \
	threads_setexcjmp(oldctx, NULL);


char *lib_strdup(const char *str)
{
	volatile size_t len;
	char *ptr = NULL;
	excjmp_context_t excctx, *oldctx;

	K_TRY(excctx, oldctx)
	{
		len = hal_strlen(str) + 1U;
	}
	K_CATCH(excctx, oldctx, return NULL);

	ptr = vm_kmalloc(len);

	if (ptr != NULL) {
		K_TRY(excctx, oldctx)
		{
			hal_memcpy(ptr, str, len);
		}
		K_CATCH(excctx, oldctx,
				vm_kfree(ptr);
				return NULL;);
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
