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


#define MAP_NONE       0x0U
#define MAP_NEEDSCOPY  (0x1U << 0)
#define MAP_UNCACHED   (0x1U << 1)
#define MAP_DEVICE     (0x1U << 2)
#define MAP_NOINHERIT  (0x1U << 3)
#define MAP_PHYSMEM    (0x1U << 4)
#define MAP_CONTIGUOUS (0x1U << 5)
#define MAP_ANONYMOUS  (0x1U << 6)
#define MAP_FIXED      (0x1U << 7)
/* NOTE: vm uses u8 to store flags, if more flags are needed this type needs to be changed. */
#define MAP_SHARED  0x0U
#define MAP_PRIVATE 0x0U


#define PROT_NONE  0x0U
#define PROT_READ  0x1U
#define PROT_WRITE 0x2U
#define PROT_EXEC  0x4U
#define PROT_USER  0x8U


#define MAP_FAILED (void *)-1


#endif
