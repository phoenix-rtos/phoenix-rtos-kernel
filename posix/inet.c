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

#include "hal/hal.h"
#include "include/errno.h"
#include "proc/proc.h"

#include "posix.h"
#include "posix_private.h"


static ssize_t socksrvcall(msg_t *msg)
{
	oid_t oid;
	ssize_t err;

	err = proc_lookup(PATH_SOCKSRV, NULL, &oid);
	if (err < 0) {
		return err;
	}

	err = proc_send(oid.port, msg);
	if (err < 0) {
		return err;
	}

	return 0;
}


static ssize_t sockcall(unsigned int socket, msg_t *msg)
{
	sockport_resp_t *smo = (void *)msg->o.raw;
	ssize_t err;

	err = proc_send(socket, msg);
	if (err < 0) {
		return err;
	}

	err = smo->ret;
	return err;
}


static ssize_t socknamecall(unsigned int socket, msg_t *msg, struct sockaddr *address, socklen_t *address_len)
{
	sockport_resp_t *smo = (void *)msg->o.raw;
	ssize_t err;

	err = sockcall(socket, msg);
	if (err < 0) {
		return err;
	}

	if (address_len != NULL) {
		if (smo->sockname.addrlen > *address_len) {
			smo->sockname.addrlen = *address_len;
		}

		hal_memcpy(address, smo->sockname.addr, smo->sockname.addrlen);
		*address_len = smo->sockname.addrlen;
	}

	return err;
}


static ssize_t sockdestcall(unsigned int socket, msg_t *msg, const struct sockaddr *address, socklen_t address_len)
{
	sockport_msg_t *smi = (void *)msg->i.raw;

	if (address_len > sizeof(smi->send.addr)) {
		return -EINVAL;
	}

	smi->send.addrlen = address_len;
	hal_memcpy(smi->send.addr, address, address_len);

	return sockcall(socket, msg);
}


int inet_accept4(unsigned int socket, struct sockaddr *address, socklen_t *address_len, unsigned int flags)
{
	msg_t msg;
	oid_t oid;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&oid, 0, sizeof(oid));
	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmAccept;
	smi->send.flags = flags;

	return (int)socknamecall(socket, &msg, address, address_len);
}


int inet_bind(unsigned int socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmBind;

	return (int)sockdestcall(socket, &msg, address, address_len);
}


int inet_connect(unsigned int socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmConnect;

	return (int)sockdestcall(socket, &msg, address, address_len);
}


int inet_getpeername(unsigned int socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetPeerName;

	return (int)socknamecall(socket, &msg, address, address_len);
}


int inet_getsockname(unsigned int socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetSockName;

	return (int)socknamecall(socket, &msg, address, address_len);
}


int inet_getsockopt(unsigned int socket, int level, int optname, void *optval, socklen_t *optlen)
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

	if (ret < 0) {
		return (int)ret;
	}

	*optlen = (socklen_t)ret;
	return 0;
}


int inet_listen(unsigned int socket, int backlog)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmListen;
	smi->listen.backlog = backlog;

	return (int)sockcall(socket, &msg);
}


ssize_t inet_recvfrom(unsigned int socket, void *message, size_t length, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len)
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


ssize_t inet_sendto(unsigned int socket, const void *message, size_t length, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSend;
	smi->send.flags = flags;
	msg.i.data = message;
	msg.i.size = length;

	return sockdestcall(socket, &msg, dest_addr, dest_len);
}


ssize_t inet_recvmsg(unsigned int socket, struct msghdr *msg, unsigned int flags)
{
	ssize_t ret = 0;

	/* multiple buffers are not supported */
	if (msg->msg_iovlen > 1) {
		return -EINVAL;
	}

	if (msg->msg_iovlen == 1) {
		ret = inet_recvfrom(socket, msg->msg_iov->iov_base, msg->msg_iov->iov_len, flags, msg->msg_name, &msg->msg_namelen);
	}

	if (ret >= 0) {
		/* control data is not supported */
		if (msg->msg_controllen > 0U) {
			msg->msg_controllen = 0;
		}

		/* output flags are not supported */
		msg->msg_flags = 0;
	}

	return ret;
}


ssize_t inet_sendmsg(unsigned int socket, const struct msghdr *msg, unsigned int flags)
{
	ssize_t ret = 0;

	/* multiple buffers are not supported */
	if (msg->msg_iovlen > 1) {
		return -EINVAL;
	}

	/* control data is not supported */
	if (msg->msg_controllen > 0U) {
		return -EINVAL;
	}

	if (msg->msg_iovlen == 1) {
		ret = inet_sendto(socket, msg->msg_iov->iov_base, msg->msg_iov->iov_len, flags, msg->msg_name, msg->msg_namelen);
	}

	return ret;
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

	err = (int)socksrvcall(&msg);
	if (err < 0) {
		return err;
	}

	return (msg.o.err < 0) ? msg.o.err : (int)msg.o.lookup.dev.port;
}


int inet_shutdown(unsigned int socket, int how)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmShutdown;
	smi->send.flags = (unsigned int)how;

	return (int)sockcall(socket, &msg);
}


int inet_setsockopt(unsigned int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSetOpt;
	smi->opt.level = level;
	smi->opt.optname = optname;
	msg.i.data = optval;
	msg.i.size = optlen;

	return (int)sockcall(socket, &msg);
}


int inet_setfl(unsigned int socket, unsigned int flags)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSetFl;
	smi->send.flags = (unsigned int)flags;

	return (int)sockcall(socket, &msg);
}


int inet_getfl(unsigned int socket)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetFl;

	return (int)sockcall(socket, &msg);
}
