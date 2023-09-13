/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX poll() symbolic constants and types
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_POLL_H_
#define _PHOENIX_POSIX_POLL_H_


/* poll() events */
#define POLLIN     0x0001 /* Data other than high priority may be read without blocking */
#define POLLRDNORM 0x0002 /* Normal data may be read without blocking */
#define POLLRDBAND 0x0004 /* Priority data may be read without blocking */
#define POLLPRI    0x0008 /* High priority data may be read without blocking */
#define POLLOUT    0x0010 /* Normal data may be written without blocking */
#define POLLWRNORM 0x0020 /* Same as POLLOUT*/
#define POLLWRBAND 0x0040 /* Priority data may be written */
#define POLLERR    0x0080 /* Error occured (output events only) */
#define POLLHUP    0x0100 /* Device disconnected (output events only) */
#define POLLNVAL   0x0200 /* Invalid file descriptor (output events only) */


typedef unsigned int nfds_t; /* Number of file descriptors */


struct pollfd {
	int fd;        /* File descriptor being polled */
	short events;  /* Input event flags */
	short revents; /* Output event flags */
};


#endif
