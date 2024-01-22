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
#define MAP_NEEDSCOPY  (1 << 0)
#define MAP_UNCACHED   (1 << 1)
#define MAP_DEVICE     (1 << 2)
#define MAP_NOINHERIT  (1 << 3)
#define MAP_PHYSMEM    (1 << 4)
#define MAP_CONTIGUOUS (1 << 5)
#define MAP_ANONYMOUS  (1 << 6)
#define MAP_FIXED      (1 << 7)
/* NOTE: vm uses u8 to store flags, if more flags are needed this type needs to be changed. */
#define MAP_SHARED     0x0
#define MAP_PRIVATE    0x0


#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4
#define PROT_USER  0x8


#define MAP_FAILED (void *)-1


#endif
