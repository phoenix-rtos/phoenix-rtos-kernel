/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Local sockets
 *
 * Copyright 2020 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_SUN_H_
#define _PROC_SUN_H_

#include "process.h"
#include "event.h"


extern int sun_close(struct _sun_t *socket);


extern int sun_socket(process_t *process, int type, int protocol, int flags);


extern int sun_bind(process_t *process, struct _sun_t *socket, const struct sockaddr *address, socklen_t address_len);


extern int sun_poll(struct _sun_t *socket, poll_head_t *poll, wait_note_t *note);


extern int sun_listen(struct _sun_t *socket, int backlog);


extern int sun_accept(process_t *process, struct _sun_t *socket, struct sockaddr *address, socklen_t *address_len);


extern int sun_connect(process_t *process, struct _sun_t *socket, const struct sockaddr *address, socklen_t address_len);


extern ssize_t sun_sendmsg(struct _sun_t *socket, const struct msghdr *msg, int flags);


extern ssize_t sun_recvmsg(struct _sun_t *socket, struct msghdr *msg, int flags);


extern ssize_t sun_read(struct _sun_t *socket, void *data, size_t size);


extern ssize_t sun_write(struct _sun_t *socket, void *data, size_t size);


void _sun_init(void);


#endif
