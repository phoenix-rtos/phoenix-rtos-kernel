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

#ifndef _PHOENIX_IOCTL_H_
#define _PHOENIX_IOCTL_H_


typedef struct {
	unsigned long request;
	char data[0];
} ioctl_in_t;


#endif
