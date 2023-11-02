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


#define MAP_NONE       0x0
#define MAP_NEEDSCOPY  0x1
#define MAP_UNCACHED   0x2
#define MAP_DEVICE     0x4
#define MAP_NOINHERIT  0x8
#define MAP_PHYSMEM    0x10
#define MAP_CONTIGUOUS 0x20
#define MAP_ANONYMOUS  0x40
#define MAP_SHARED     0x0
#define MAP_PRIVATE    0x0
#define MAP_FIXED      0x0


#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4
#define PROT_USER  0x8


#define MAP_FAILED (void *)-1


#endif
