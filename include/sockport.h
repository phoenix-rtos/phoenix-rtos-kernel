/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Socket port definitions
 *
 * Copyright 2018 Phoenix Systems
 * Author: Michał Mirosław
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SOCKPORT_H_
#define _PH_SOCKPORT_H_

#include "msg.h"


/* clang-format off */
enum {
	sockmSocket = 0x50c30000, sockmShutdown,
	sockmConnect, sockmBind, sockmListen, sockmAccept,
	sockmSend, sockmRecv, sockmGetSockName, sockmGetPeerName,
	sockmGetFl, sockmSetFl, sockmGetOpt, sockmSetOpt,
	sockmGetNameInfo, sockmGetAddrInfo, sockmGetIfAddrs,
};
/* clang-format on */

#define SOCKPORT_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX_SOCKNAME_LEN   (SOCKPORT_MIN(sizeof(((msg_t *)NULL)->i.raw), sizeof(((msg_t *)NULL)->o.raw)) - (sizeof(unsigned int) /* flags */ + sizeof(size_t) /* addrlen */))

#define PATH_SOCKSRV "/dev/netsocket"


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
		unsigned int flags;
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
			int err;
			size_t buflen;
		} sys;
	};
} sockport_resp_t;


#undef SOCKPORT_MIN

#endif
