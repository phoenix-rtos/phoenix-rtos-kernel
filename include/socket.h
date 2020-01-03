/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * socket.h
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski, Michał Mirosław
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SOCKET_H_
#define _PHOENIX_SOCKET_H_

#include "types.h"

#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_INET AF_INET
#define PF_INET6 AF_INET6

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_SEQPACKET 4
#define SOCK_RDM 5

#define SOL_SOCKET 0xFFF
#define SO_ACCEPTCONN 0x0002
#define SO_BROADCAST 0x0020
#define SO_DEBUG 0x0001
#define SO_DONTROUTE 0x0010
#define SO_ERROR 0x1007
#define SO_KEEPALIVE 0x0008
#define SO_LINGER 0x0080
#define SO_OOBINLINE 0x0100
#define SO_RCVBUF 0x1002
#define SO_RCVLOWAT 0x1004
#define SO_RCVTIMEO 0x1006
#define SO_REUSEADDR 0x0004
#define SO_SNDBUF 0x1001
#define SO_SNDLOWAT 0x1003
#define SO_SNDTIMEO 0x1005
#define SO_TYPE 0x1008

#define MSG_OOB  0x01
#define MSG_PEEK 0x02
#define MSG_DONTWAIT 0x08
#define MSG_TRUNC 0x10

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define SCM_RIGHTS 1


typedef int socklen_t;
typedef unsigned short sa_family_t;


struct sockaddr {
	sa_family_t sa_family;
	char        sa_data[14];
};


struct msghdr {
	void         *msg_name;
	socklen_t     msg_namelen;
	struct iovec *msg_iov;
	unsigned int /* TODO: size_t */        msg_iovlen;
	void         *msg_control;
	unsigned int /* TODO: size_t */        msg_controllen;
	int           msg_flags;
};


#endif
