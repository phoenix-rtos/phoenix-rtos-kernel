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


extern int socket_accept(port_t *port, id_t socket, struct sockaddr *address, socklen_t *address_len, int flags);


extern int socket_bind(port_t *port, id_t socket, const struct sockaddr *address, socklen_t address_len);


extern int socket_connect(port_t *port, id_t socket, const struct sockaddr *address, socklen_t address_len);


extern int socket_getpeername(port_t *port, id_t socket, struct sockaddr *address, socklen_t *address_len);


extern int socket_getsockname(port_t *port, id_t socket, struct sockaddr *address, socklen_t *address_len);


extern int socket_getsockopt(port_t *port, id_t socket, int level, int optname, void *optval, socklen_t *optlen);


extern int socket_listen(port_t *port, id_t socket, int backlog);


extern ssize_t socket_recvfrom(port_t *port, id_t socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t socket_sendto(port_t *port, id_t socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern int socket_create(int domain, int type, int protocol);


extern int socket_shutdown(port_t *port, id_t socket, int how);


extern int socket_setsockopt(port_t *port, id_t socket, int level, int optname, const void *optval, socklen_t optlen);

#endif
