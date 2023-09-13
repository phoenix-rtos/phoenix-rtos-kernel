/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX sockets port
 *
 * Copyright 2018, 2021 Phoenix Systems
 * Author: Michał Mirosław, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_SOCKPORT_H_
#define _PHOENIX_POSIX_SOCKPORT_H_

#include "types.h"
#include "../msg.h"


#define PATH_SOCKSRV "/dev/netsocket"
#define MAX_SOCKNAME_LEN (sizeof(((msg_t *)0)->o.raw) - sizeof(ssize_t) - sizeof(size_t))


enum {
	sockmSocket = 0x50c30000, sockmShutdown,
	sockmConnect, sockmBind, sockmListen, sockmAccept,
	sockmSend, sockmRecv, sockmGetSockName, sockmGetPeerName,
	sockmGetFl, sockmSetFl, sockmGetOpt, sockmSetOpt,
	sockmGetNameInfo, sockmGetAddrInfo, sockmGetIfAddrs
};


typedef union {
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


typedef struct {
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
			int err;
			size_t buflen;
		} sys;
	};
} sockport_resp_t;


#endif
