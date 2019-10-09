/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Sockets
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_SOCKET_H_
#define _PROC_SOCKET_H_

#include HAL
#include "../../include/socket.h"


extern int socket_accept(const oid_t *oid, struct sockaddr *address, socklen_t *address_len, int flags);


extern int socket_bind(const oid_t *oid, const struct sockaddr *address, socklen_t address_len);


extern int socket_connect(const oid_t *oid, const struct sockaddr *address, socklen_t address_len);


extern int socket_getpeername(const oid_t *oid, struct sockaddr *address, socklen_t *address_len);


extern int socket_getsockname(const oid_t *oid, struct sockaddr *address, socklen_t *address_len);


extern int socket_getsockopt(const oid_t *oid, int level, int optname, void *optval, socklen_t *optlen);


extern int socket_listen(const oid_t *oid, int backlog);


extern ssize_t socket_recvfrom(const oid_t *oid, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t socket_sendto(const oid_t *oid, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern int socket_create(oid_t *oid, int domain, int type, int protocol);


extern int socket_shutdown(const oid_t *oid, int how);


extern int socket_setsockopt(const oid_t *oid, int level, int optname, const void *optval, socklen_t optlen);

#endif
