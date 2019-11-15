/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Sockets
 *
 * Copyright 2018, 2019 Phoenix Systems
 * Author: Michal Miroslaw, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../../include/socket.h"
#include "../proc/proc.h"

#if 0


int socket_accept(port_t *port, id_t socket, struct sockaddr *address, socklen_t *address_len, int flags)
{
	ssize_t err;
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtAccept;
	smi->send.flags = 0;

	if ((err = socknamecall(port, socket, &msg, address, address_len)) < 0)
		return err;

	return err;
}


int socket_bind(port_t *port, id_t socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtBind;

	return sockdestcall(port, socket, &msg, address, address_len);
}


int socket_connect(port_t *port, id_t socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtConnect;

	return sockdestcall(port, socket, &msg, address, address_len);
}


int socket_listen(port_t *port, id_t socket, int backlog)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtListen;
	smi->listen.backlog = backlog;

	return sockcall(port, socket, &msg);
}
#endif