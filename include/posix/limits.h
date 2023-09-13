/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX implemetation defined constants
 *
 * Copyright 2021 Phoenix Systems
 * Author: Ziemowit Leszczynski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_LIMITS_H_
#define _PHOENIX_POSIX_LIMITS_H_

#include "../arch.h"


#define PAGE_SIZE _PAGE_SIZE /* Size in bytes of a page */
#define PAGESIZE  PAGE_SIZE  /* Same as PAGE_SIZE */

#define HOST_NAME_MAX 255  /* Maximum length of a host name (not including the terminating null) */
#define IOV_MAX       1024 /* Maximum number of iovec structures that one process has available */


#endif
