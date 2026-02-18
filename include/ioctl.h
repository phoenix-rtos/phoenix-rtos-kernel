/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ioctl
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_IOCTL_H_
#define _PH_IOCTL_H_

#define IOCPARM_MASK   0x1fffUL
#define IOCPARM_LEN(x) (((x) >> 16) & IOCPARM_MASK)
#define IOCBASECMD(x)  ((x) & ~(IOCPARM_MASK << 16))
#define IOCGROUP(x)    (((x) >> 8) & 0xffU)

#define IOC_VOID    0x00000000UL
#define IOC_NESTED  0x20000000UL
#define IOC_OUT     0x40000000UL
#define IOC_IN      0x80000000UL
#define IOC_INOUT   (IOC_IN | IOC_OUT)
#define IOC_DIRMASK 0xc0000000UL

#define _IOC(inout, group, num, len) ((unsigned long)((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | (num)))

typedef struct {
	unsigned long request;
	unsigned long size;
	char data[];
} ioctl_in_t;


#endif
