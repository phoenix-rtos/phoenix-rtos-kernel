/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System types
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_TYPES_H_
#define _PHOENIX_TYPES_H_

#include <stddef.h>
#include "arch.h"

/* Include platform dependent types */
#ifdef __PHOENIX_ARCH_TYPES
#include __PHOENIX_ARCH_TYPES
#endif


/* Types shared across all architectures */
typedef unsigned int handle_t;     /* Generic system resource handler, should contain resource_t id value (28-bit) */
typedef unsigned long long time_t; /* Generic system time type (also defined by POSIX, should store time in seconds) */
typedef long long off_t;           /* Generic offset type (also defined by POSIX, used to represent file size) */
typedef long long id_t;            /* Generic system identifier (also defined by POSIX, should contain other ID types like pid_t, gid_t, uid_t and dev_t) */

/* Object ID (used to identify objects within servers connected to a given port) */
typedef struct {
	unsigned int port; /* Server port */
	id_t id;           /* Object ID */
} oid_t;


#endif
