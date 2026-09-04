/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module, UNIX sockets
 *
 * Copyright 2026 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_POSIX_USOCKET_H_
#define _PH_POSIX_USOCKET_H_

#include "hal/hal.h"
#include "proc/proc.h"
#include "posix.h"

/*
 * This define is used against oid_t.id which is __u32
 * hence the implicit bit value instead of -1.
 */
#define USOCKET_PORT 0xffffffffU /* FIXME */


typedef struct _usocket_t usocket_t;


/*
 * A UNIX socket is addressed by a reference-carrying handle rather than by an
 * id, so that a descriptor keeps the socket alive and no lookup is needed on
 * the data path. The object is defined in unix.c.
 */

int usocket_accept4(usocket_t *ls, struct sockaddr *address, socklen_t *address_len, unsigned int flags, usocket_t **s);


int usocket_bind(usocket_t *s, const struct sockaddr *address, socklen_t address_len);


int usocket_connect(usocket_t *s, const struct sockaddr *address, socklen_t address_len);


int usocket_getpeername(usocket_t *s, struct sockaddr *address, socklen_t *address_len);


int usocket_getsockname(usocket_t *s, struct sockaddr *address, socklen_t *address_len);


int usocket_getsockopt(usocket_t *s, int level, int optname, void *optval, socklen_t *optlen);


int usocket_listen(usocket_t *s, int backlog);


ssize_t usocket_recvfrom(usocket_t *s, void *msg, size_t len, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len);


ssize_t usocket_sendto(usocket_t *s, const void *msg, size_t len, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


ssize_t usocket_recvmsg(usocket_t *s, struct msghdr *msg, unsigned int flags);


ssize_t usocket_sendmsg(usocket_t *s, const struct msghdr *msg, unsigned int flags);


int usocket_socket(int domain, unsigned int type, int protocol, usocket_t **s);


int usocket_socketpair(int domain, unsigned int type, int protocol, usocket_t *sv[2]);


int usocket_shutdown(usocket_t *s, int how);


/* Detaches a socket from a name whose socket file has just been removed. */
int usocket_unlink(id_t id);


int usocket_setsockopt(usocket_t *s, int level, int optname, const void *optval, socklen_t optlen);


int usocket_setfl(usocket_t *s, unsigned int flags);


int usocket_getfl(usocket_t *s);


int usocket_close(usocket_t *s);


int usocket_poll(usocket_t *s, unsigned short events);


void usocket_init(void);


#endif
