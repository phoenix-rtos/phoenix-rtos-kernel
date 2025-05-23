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


typedef struct {
	unsigned long request;
	unsigned long size;
	char data[];
} ioctl_in_t;


#endif
