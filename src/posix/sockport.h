/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/sockport.h
 *
 * Copyright 2018 Phoenix Systems
 * Author: Michał Mirosław
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _SYS_SOCKPORT_H_
#define _SYS_SOCKPORT_H_

#include "../proc/msg.h"
#include "sockdefs.h"


enum {
	sockmSocket = 0x50c30000, sockmShutdown,
	sockmConnect, sockmBind, sockmListen, sockmAccept,
	sockmSend, sockmRecv, sockmGetSockName, sockmGetPeerName,
	sockmGetFl, sockmSetFl, sockmGetOpt, sockmSetOpt,
	sockmGetNameInfo, sockmGetAddrInfo,
};

#define PATH_SOCKSRV "/dev/netsocket"


typedef union sockport_msg_ {
	struct {
		int domain, type, protocol, flags;
		size_t ai_node_sz;
	} socket;
	struct {
		id_t sock_id;
		int backlog;
	} listen;
	struct {
		id_t sock_id;
		int level;
		int optname;
	} opt;
	struct {
		id_t sock_id;
		size_t addrlen;
		int flags;
		char addr[0];
	} send;
} sockport_msg_t;


typedef struct sockport_resp_ {
	ssize_t ret;
	union {
		struct {
			oid_t socket;
			size_t addrlen;
			char addr[0];
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


enum {
	__MAX_SEND_SOCKNAME_LEN = sizeof(((msg_t *)0)->i.raw) - sizeof(oid_t) - sizeof(size_t) - sizeof(int),
	__MAX_SOCKNAME_ADDR_LEN = sizeof(((msg_t *)0)->o.raw) - sizeof(oid_t) - 2 * sizeof(size_t),
	MAX_SOCKNAME_LEN = __MAX_SEND_SOCKNAME_LEN < __MAX_SOCKNAME_ADDR_LEN ? __MAX_SEND_SOCKNAME_LEN : __MAX_SOCKNAME_ADDR_LEN,
};


#endif
