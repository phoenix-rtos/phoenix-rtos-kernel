/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ioctl
 *
 * Copyright 2018, 2021 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_IOCTL_H_
#define _PHOENIX_IOCTL_H_

#define IOCPARM_MASK   0x1fff                        /* Parameter mask, 13 bits */
#define IOCPARM_LEN(x) (((x) >> 16) & IOCPARM_MASK)  /* Parameter length */
#define IOCBASECMD(x)  ((x) & ~(IOCPARM_MASK << 16)) /* Command */
#define IOCGROUP(x)    (((x) >> 8) & 0xff)           /* Group */

#define IOC_VOID    0x20000000         /* No parameters */
#define IOC_OUT     0x40000000         /* Copy out parameters */
#define IOC_IN      0x80000000         /* Copy in parameters */
#define IOC_INOUT   (IOC_IN | IOC_OUT) /* Copy in and out parameters */
#define IOC_DIRMASK 0xe0000000         /* Mask for IOC_VOID | IOC_OUT | IOC_IN */

#define _IOC(inout,group,num,len) ((unsigned long)(inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num)))

#define _IO(g,n)     _IOC(IOC_VOID, (g), (n), 0)
#define _IOV(g,n,t)  _IOC(IOC_VOID, (g), (n), sizeof(t))
#define _IOR(g,n,t)  _IOC(IOC_OUT, (g), (n), sizeof(t))
#define _IOW(g,n,t)  _IOC(IOC_IN, (g), (n), sizeof(t))
#define _IOWR(g,n,t) _IOC(IOC_INOUT, (g), (n), sizeof(t))


typedef struct {
	id_t id;
	unsigned pid;
	unsigned long request;
	char data[];
} ioctl_in_t;


typedef struct {
	int err;
	char data[];
} ioctl_out_t;


#endif
