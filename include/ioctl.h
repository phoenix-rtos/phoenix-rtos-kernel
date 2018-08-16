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
