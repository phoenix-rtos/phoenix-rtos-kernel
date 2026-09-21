/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * UNIX socket address
 *
 * Copyright 2026 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_POSIX_UADDR_H_
#define _PH_POSIX_UADDR_H_

#include "include/posix-socket.h"


/*
 * A name a socket has been bound to, as getsockname() reports it: the family,
 * the path and its terminating null. It never changes once it has been
 * published, and it is reference counted, which is what lets the two ends of a
 * connection know each other's name without ever looking at each other's
 * endpoint - a connector takes the listener's name while it still holds the
 * listener, and accept() takes the connector's while it holds its lock. It is
 * also what a datagram carries through a channel, so that recvfrom() can say
 * who sent it even once the sender is gone.
 */
typedef struct {
	int refs;      /* atomic */
	socklen_t len; /* sizeof(sa_family_t) + strlen(path) + 1 */
	char path[];
} uaddr_t;


/* Makes the name of `path` with one reference. */
uaddr_t *uaddr_alloc(const char *path);


uaddr_t *uaddr_ref(uaddr_t *addr);


/* Drops one reference, freeing the name along with the last of them. NULL is allowed. */
void uaddr_put(uaddr_t *addr);


/*
 * Answers getsockname(), getpeername(), accept() and recvfrom() the POSIX
 * way: the caller's buffer is filled as far as it goes, while `address_len`
 * is set to the length the address really has, so that a truncated answer can
 * be told apart from a complete one. A socket with no name of its own answers
 * with the family alone.
 */
void uaddr_copy(const uaddr_t *addr, struct sockaddr *address, socklen_t *address_len);

#endif
