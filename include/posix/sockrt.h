/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX sockets routing table entry
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_SOCKRT_H_
#define _PHOENIX_POSIX_SOCKRT_H_

#include "socket.h"


/* Route entry, used for SIOCADDRT and SIOCDELRT requests */
struct rtentry
{
	struct sockaddr rt_dst;
	struct sockaddr rt_gateway;
	struct sockaddr rt_genmask;
	unsigned short rt_flags;
	short rt_metric;
	char *rt_dev;
	unsigned long rt_mss;
	unsigned long rt_window;
	unsigned short rt_irtt;
};


#endif
