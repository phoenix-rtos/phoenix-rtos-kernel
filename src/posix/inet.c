/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module, inet sockets
 *
 * Copyright 2018 Phoenix Systems
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
		return set_errno(err);

	if ((err = proc_send(oid.port, msg)) < 0)
		return set_errno(err);

	return 0;
}


static ssize_t sockcall(unsigned socket, msg_t *msg)
{
	sockport_resp_t *smo = (void *)msg->o.raw;
	int err;

	if ((err = proc_send(socket, msg)) < 0)
		return set_errno(err);

	err = smo->ret;
	return set_errno(err);
}


static ssize_t socknamecall(unsigned socket, msg_t *msg, struct sockaddr *address, socklen_t *address_len)
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


static ssize_t sockdestcall(unsigned socket, msg_t *msg, const struct sockaddr *address, socklen_t address_len)
{
	sockport_msg_t *smi = (void *)msg->i.raw;

	if (address_len > sizeof(smi->send.addr))
		return set_errno(-EINVAL);

	smi->send.addrlen = address_len;
	hal_memcpy(smi->send.addr, address, address_len);

	return sockcall(socket, msg);
}


int inet_accept(unsigned socket, struct sockaddr *address, socklen_t *address_len)
{
	process_info_t *p;
	ssize_t err;
	msg_t msg;
	oid_t oid;
	sockport_msg_t *smi = (void *)msg.i.raw;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&oid, 0, sizeof(oid));
	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmAccept;
	smi->send.flags = 0;

	if ((err = socknamecall(socket, &msg, address, address_len)) < 0)
		return set_errno(err);

	oid.port = err;

	if ((err = posix_newFile(p, 0)) < 0) {
		msg.type = mtClose;
		proc_send(oid.port, &msg);
		return set_errno(err);
	}

	p->fds[err].file->type = ftInetSocket;
	hal_memcpy(&p->fds[err].file->oid, &oid, sizeof(oid));

	return err;
}


int inet_bind(unsigned socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmBind;

	return sockdestcall(socket, &msg, address, address_len);
}


int inet_connect(unsigned socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmConnect;

	return sockdestcall(socket, &msg, address, address_len);
}


int inet_getpeername(unsigned socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetPeerName;

	return socknamecall(socket, &msg, address, address_len);
}


int inet_getsockname(unsigned socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetSockName;

	return socknamecall(socket, &msg, address, address_len);
}


int inet_getsockopt(unsigned socket, int level, int optname, void *optval, socklen_t *optlen)
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


int inet_listen(unsigned socket, int backlog)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmListen;
	smi->listen.backlog = backlog;

	return sockcall(socket, &msg);
}


ssize_t inet_recvfrom(unsigned socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
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


ssize_t inet_sendto(unsigned socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
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


int inet_socket(int domain, int type, int protocol)
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

	return msg.o.lookup.err < 0 ? msg.o.lookup.err : msg.o.lookup.dev.port;
}


int inet_shutdown(unsigned socket, int how)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmShutdown;
	smi->send.flags = how;

	return sockcall(socket, &msg);
}


int inet_setsockopt(unsigned socket, int level, int optname, const void *optval, socklen_t optlen)
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
