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


#define POLLIN     0x1U
#define POLLRDNORM 0x2U
#define POLLRDBAND 0x4U
#define POLLPRI    0x8U
#define POLLOUT    0x10U
#define POLLWRNORM 0x20U
#define POLLWRBAND 0x40U
#define POLLERR    0x80U
#define POLLHUP    0x100U
#define POLLNVAL   0x200U


typedef unsigned int nfds_t;


struct pollfd {
	int fd;
	short events;
	short revents;
};


#endif
