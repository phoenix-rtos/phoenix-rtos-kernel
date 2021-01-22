/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Memory management
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_MMAN_H_
#define _PHOENIX_MMAN_H_


enum { MAP_NONE = 0x0, MAP_NEEDSCOPY = 0x1, MAP_UNCACHED = 0x2, MAP_DEVICE = 0x4, MAP_NOINHERIT = 0x8,
	MAP_SHARED = 0x0, MAP_PRIVATE = 0x0, MAP_FIXED = 0x0, MAP_ANONYMOUS = 0x0 };


enum { PROT_NONE = 0x0, PROT_READ = 0x1, PROT_WRITE = 0x2, PROT_EXEC = 0x4, PROT_USER = 0x8 };


/* Predefined oids */
#define OID_NULL       NULL
#define OID_PHYSMEM    (void *)-1
#define OID_CONTIGUOUS (void *)-2


#endif
