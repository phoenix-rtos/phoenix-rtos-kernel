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

/* TODO: share this definition with network stack */
enum { MAX_SOCKNAME_LEN = sizeof(((msg_t *)0)->o.raw) - 2 * sizeof(size_t) };

typedef union sockport_msg_ {
	struct {
		int domain, type, protocol, flags;
		size_t ai_node_sz;
	} socket;
	struct {
		int backlog;
	} listen;
	struct {
		int level;
		int optname;
	} opt;
	struct {
		int flags;
		size_t addrlen;
		char addr[MAX_SOCKNAME_LEN];
	} send;
} sockport_msg_t;


typedef struct sockport_resp_ {
	ssize_t ret;
	union {
		struct {
			size_t addrlen;
			char addr[MAX_SOCKNAME_LEN];
		} sockname;
		struct {
			size_t hostlen;
			size_t servlen;
		} nameinfo;
		struct {
			int errno;
			size_t buflen;
		} sys;
	};
} sockport_resp_t;


static ssize_t sockcall(const oid_t *oid, msg_t *msg)
{
	sockport_resp_t *smo = (void *)msg->o.raw;
	int err;
	msg->object = oid->id;

	if ((err = proc_send(oid->port, msg)) < 0)
		return err;

	err = smo->ret;
	return err;
}


static ssize_t socknamecall(const oid_t *oid, msg_t *msg, struct sockaddr *address, socklen_t *address_len)
{
	sockport_resp_t *smo = (void *)msg->o.raw;
	ssize_t err;
	msg->object = oid->id;

	if ((err = sockcall(oid, msg)) < 0)
		return err;

	if (address_len != NULL) {
		if (smo->sockname.addrlen > *address_len)
			smo->sockname.addrlen = *address_len;

		hal_memcpy(address, smo->sockname.addr, smo->sockname.addrlen);
		*address_len = smo->sockname.addrlen;
	}

	return err;
}


static ssize_t sockdestcall(const oid_t *oid, msg_t *msg, const struct sockaddr *address, socklen_t address_len)
{
	sockport_msg_t *smi = (void *)msg->i.raw;
	msg->object = oid->id;

	if (address_len > sizeof(smi->send.addr))
		return -EINVAL;

	smi->send.addrlen = address_len;
	hal_memcpy(smi->send.addr, address, address_len);

	return sockcall(oid, msg);
}


int socket_accept(const oid_t *oid, struct sockaddr *address, socklen_t *address_len, int flags)
{
	ssize_t err;
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtAccept;
	smi->send.flags = 0;

	if ((err = socknamecall(oid, &msg, address, address_len)) < 0)
		return err;

	return err;
}


int socket_bind(const oid_t *oid, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtBind;

	return sockdestcall(oid, &msg, address, address_len);
}


int socket_connect(const oid_t *oid, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtConnect;

	return sockdestcall(oid, &msg, address, address_len);
}


int socket_getpeername(const oid_t *oid, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtGetPeerName;

	return socknamecall(oid, &msg, address, address_len);
}


int socket_getsockname(const oid_t *oid, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtGetSockName;

	return socknamecall(oid, &msg, address, address_len);
}


int socket_getsockopt(const oid_t *oid, int level, int optname, void *optval, socklen_t *optlen)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;
	ssize_t ret;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtGetOpt;
	smi->opt.level = level;
	smi->opt.optname = optname;
	msg.o.data = optval;
	msg.o.size = *optlen;

	ret = sockcall(oid, &msg);

	if (ret < 0)
		return ret;

	*optlen = ret;
	return 0;
}


int socket_listen(const oid_t *oid, int backlog)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtListen;
	smi->listen.backlog = backlog;

	return sockcall(oid, &msg);
}


ssize_t socket_recvfrom(const oid_t *oid, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtRecv;
	smi->send.flags = flags;
	msg.o.data = message;
	msg.o.size = length;

	return socknamecall(oid, &msg, src_addr, src_len);
}


ssize_t socket_sendto(const oid_t *oid, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtSend;
	smi->send.flags = flags;
	msg.i.data = (void *)message;
	msg.i.size = length;

	return sockdestcall(oid, &msg, dest_addr, dest_len);
}


int socket_create(oid_t *oid, int domain, int type, int protocol)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;
	int err;
	file_t *srv;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtSocket;
	smi->socket.domain = domain;
	smi->socket.type = type;
	smi->socket.protocol = protocol;

	if ((err = file_open(&srv, proc_current()->process, -1, "/dev/netsocket", 0, 0)) != EOK)
		return err;

	err = port_send(srv->port, &msg);
	file_put(srv);

	if (err != EOK)
		return err;

	oid->port = msg.o.io;
	oid->id = 0;

	return EOK;
}


int socket_shutdown(const oid_t *oid, int how)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtShutdown;
	smi->send.flags = how;

	return sockcall(oid, &msg);
}


int socket_setsockopt(const oid_t *oid, int level, int optname, const void *optval, socklen_t optlen)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtSetOpt;
	smi->opt.level = level;
	smi->opt.optname = optname;
	msg.i.data = (void *)optval;
	msg.i.size = optlen;

	return sockcall(oid, &msg);
}
