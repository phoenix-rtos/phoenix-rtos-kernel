/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - poll
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_POLL_H_
#define _PHOENIX_POSIX_POLL_H_


#define POLLIN     0x1
#define POLLRDNORM 0x2
#define POLLRDBAND 0x4
#define POLLPRI    0x8
#define POLLOUT    0x10
#define POLLWRNORM 0x20
#define POLLWRBAND 0x40
#define POLLERR    0x80
#define POLLHUP    0x100
#define POLLNVAL   0x200


typedef unsigned int nfds_t;


struct pollfd {
	int fd;
	short events;
	short revents;
};


#endif
