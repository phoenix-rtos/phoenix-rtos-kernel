/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module, inet sockets
 *
 * Copyright 2018-2019 Phoenix Systems
 * Author: Michal Miroslaw, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../proc/proc.h"

#include "posix.h"
#include "posix_private.h"


static int socksrvcall(msg_t *msg)
{
	oid_t oid;
	int err;

	if ((err = proc_lookup(PATH_SOCKSRV, NULL, &oid)) < 0)
		return err;

	if ((err = proc_send(oid.port, msg)) < 0)
		return err;

	return 0;
}


static ssize_t sockcall(const oid_t *socket, msg_t *msg)
{
	sockport_msg_t *smi = (void *)msg->i.raw;
	sockport_resp_t *smo = (void *)msg->o.raw;
	int err;

	smi->send.sock_id = socket->id;
	if ((err = proc_send(socket->port, msg)) < 0)
		return err;

	return smo->ret;
}


static ssize_t socknamecall(const oid_t *socket, msg_t *msg, struct sockaddr *address, socklen_t *address_len)
{
	sockport_resp_t *smo = (void *)msg->o.raw;
	ssize_t err;

	if ((err = sockcall(socket, msg)) < 0)
		return err;

	if (address_len != NULL) {
		if (smo->sockname.addrlen > *address_len)
			smo->sockname.addrlen = *address_len;

		hal_memcpy(address, smo->sockname.addr, smo->sockname.addrlen);
		*address_len = smo->sockname.addrlen;
	}

	return err;
}


static ssize_t sockdestcall(const oid_t *socket, msg_t *msg, const struct sockaddr *address, socklen_t address_len)
{
	sockport_msg_t *smi = (void *)msg->i.raw;

	if (address_len > MAX_SOCKNAME_LEN)
		return -EINVAL;

	smi->send.addrlen = address_len;
	hal_memcpy(smi->send.addr, address, address_len);

	return sockcall(socket, msg);
}


int inet_accept4(const oid_t *socket, oid_t *new_sock, struct sockaddr *address, socklen_t *address_len, int flags)
{
	ssize_t err;
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;
	sockport_resp_t *smo = (void *)msg.o.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmAccept;
	smi->send.flags = flags;

	if ((err = socknamecall(socket, &msg, address, address_len)) < 0)
		return err;

	hal_memcpy(new_sock, &smo->sockname.socket, sizeof(*new_sock));

	return 0;
}


int inet_bind(const oid_t *socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmBind;

	return sockdestcall(socket, &msg, address, address_len);
}


int inet_connect(const oid_t *socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmConnect;

	return sockdestcall(socket, &msg, address, address_len);
}


int inet_getpeername(const oid_t *socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetPeerName;

	return socknamecall(socket, &msg, address, address_len);
}


int inet_getsockname(const oid_t *socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetSockName;

	return socknamecall(socket, &msg, address, address_len);
}


int inet_getsockopt(const oid_t *socket, int level, int optname, void *optval, socklen_t *optlen)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;
	ssize_t ret;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetOpt;
	smi->opt.level = level;
	smi->opt.optname = optname;
	msg.o.data = optval;
	msg.o.size = *optlen;

	ret = sockcall(socket, &msg);

	if (ret < 0)
		return ret;

	*optlen = ret;
	return 0;
}


int inet_listen(const oid_t *socket, int backlog)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmListen;
	smi->listen.backlog = backlog;

	return sockcall(socket, &msg);
}


ssize_t inet_recvfrom(const oid_t *socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmRecv;
	smi->send.flags = flags;
	msg.o.data = message;
	msg.o.size = length;

	return socknamecall(socket, &msg, src_addr, src_len);
}


ssize_t inet_sendto(const oid_t *socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSend;
	smi->send.flags = flags;
	msg.i.data = (void *)message;
	msg.i.size = length;

	return sockdestcall(socket, &msg, dest_addr, dest_len);
}


int inet_socket(oid_t *socket, int domain, int type, int protocol)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;
	int err;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSocket;
	smi->socket.domain = domain;
	smi->socket.type = type;
	smi->socket.protocol = protocol;

	if ((err = socksrvcall(&msg)) < 0)
		return err;

	hal_memcpy(socket, &msg.o.lookup.dev, sizeof(*socket));
	return msg.o.lookup.err;
}


int inet_shutdown(const oid_t *socket, int how)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmShutdown;
	smi->send.flags = how;

	return sockcall(socket, &msg);
}


int inet_setsockopt(const oid_t *socket, int level, int optname, const void *optval, socklen_t optlen)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSetOpt;
	smi->opt.level = level;
	smi->opt.optname = optname;
	msg.i.data = (void *)optval;
	msg.i.size = optlen;

	return sockcall(socket, &msg);
}


int inet_getfl(const oid_t *socket)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetFl;

	return sockcall(socket, &msg);
}


int inet_setfl(const oid_t *socket, int val)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSetFl;
	smi->send.flags = val;

	return sockcall(socket, &msg);
}
