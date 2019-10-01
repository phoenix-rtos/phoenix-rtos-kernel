/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * poll.h
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POLL_H_
#define _PHOENIX_POLL_H_

#define POLLRDNORM     (1 << 0)
#define POLLRDBAND     (1 << 1)
#define POLLPRI        (1 << 2)
#define POLLIN         (POLLRDNORM | POLLRDBAND)

#define POLLWRNORM    (1 << 3)
#define POLLWRBAND    (1 << 4)
#define POLLOUT       (POLLWRNORM)

#define POLLERR      (1 << 5)
#define POLLHUP      (1 << 6)
#define POLLNVAL     (1 << 7)

#define POLLIN_SET  (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#define POLLOUT_SET (POLLWRNORM | POLLWRBAND | POLLOUT | POLLHUP)
#define POLLEX_SET  (POLLPRI)
#define POLLIGN_SET (POLLERR | POLLHUP | POLLNVAL)


typedef unsigned int nfds_t;


struct pollfd {
	int   fd;
	short events;
	short revents;
};

#endif
