/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - socket
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_POSIX_SOCKET_H_
#define _PH_POSIX_SOCKET_H_


#include "types.h"


#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6  10
#define AF_KEY    15
#define AF_PACKET 17

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6
#define PF_KEY    AF_KEY
#define PF_PACKET AF_PACKET

#define SOCK_STREAM    1U
#define SOCK_DGRAM     2U
#define SOCK_RAW       3U
#define SOCK_SEQPACKET 4U
#define SOCK_RDM       5U

#define SOL_SOCKET 0xFFF

#define SO_ACCEPTCONN   0x0002U
#define SO_BROADCAST    0x0020U
#define SO_DEBUG        0x0001U
#define SO_DONTROUTE    0x0010U
#define SO_ERROR        0x1007U
#define SO_KEEPALIVE    0x0008U
#define SO_LINGER       0x0080U
#define SO_OOBINLINE    0x0100U
#define SO_RCVBUF       0x1002U
#define SO_RCVLOWAT     0x1004U
#define SO_RCVTIMEO     0x1006U
#define SO_REUSEADDR    0x0004U
#define SO_SNDBUF       0x1001U
#define SO_SNDLOWAT     0x1003U
#define SO_SNDTIMEO     0x1005U
#define SO_TYPE         0x1008U
#define SO_BINDTODEVICE 0x100bU

#define MSG_PEEK     0x01U
#define MSG_WAITALL  0x02U
#define MSG_OOB      0x04U
#define MSG_DONTWAIT 0x08U
#define MSG_MORE     0x10U
#define MSG_NOSIGNAL 0x20U

#define SCM_RIGHTS 1

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2


typedef size_t socklen_t;
typedef __u16 sa_family_t;

struct sockaddr {
	sa_family_t sa_family;
	char sa_data[14];
};


struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
	int msg_iovlen;
	void *msg_control;
	socklen_t msg_controllen;
	int msg_flags;
};


struct cmsghdr {
	socklen_t cmsg_len;
	int cmsg_level;
	int cmsg_type;
};


#endif
