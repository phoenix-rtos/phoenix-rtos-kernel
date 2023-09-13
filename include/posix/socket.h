/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX sockets symbolic constants and types
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_SOCKET_H_
#define _PHOENIX_POSIX_SOCKET_H_

#include "iovec.h"
#include "types.h"


/* Socket types */
#define SOCK_STREAM    1 /* Byte stream socket */
#define SOCK_DGRAM     2 /* Datagram socket */
#define SOCK_RAW       3 /* Raw socket */
#define SOCK_SEQPACKET 4 /* Sequenced packet socket */
#define SOCK_RDM       5 /* Reliably Delivered Message */


/* Additional socket behaviour options combined with socket type */
#define SOCK_CLOEXEC  0x4000 /* Set FD_CLOEXEC flag */
#define SOCK_NONBLOCK 0x8000 /* Set O_NONBLOCK file status flag */


/* Socket level for getsockopt()/setsockopt() */
#define SOL_SOCKET 0xfff /* Use options below at socket level */


/* Socket level options */
#define SO_DEBUG        0x0001 /* Debugging information is being recorded */
#define SO_ACCEPTCONN   0x0002 /* Socket is accepting connections */
#define SO_REUSEADDR    0x0004 /* Reuse of local addresses is supported */
#define SO_KEEPALIVE    0x0008 /* Connections are kept alive with periodic messages */
#define SO_DONTROUTE    0x0010 /* Bypass normal routing */
#define SO_BROADCAST    0x0020 /* Transmission of broadcast messages is supported */
#define SO_LINGER       0x0080 /* Socket lingers on close */
#define SO_OOBINLINE    0x0100 /* Out of band data is transmitted in line */
#define SO_SNDBUF       0x1001 /* Send buffer size */
#define SO_RCVBUF       0x1002 /* Receive buffer size */
#define SO_SNDLOWAT     0x1003 /* Send 'low water mark' */
#define SO_RCVLOWAT     0x1004 /* Receive 'low water mark' */
#define SO_SNDTIMEO     0x1005 /* Send timeout */
#define SO_RCVTIMEO     0x1006 /* Receive timeout */
#define SO_ERROR        0x1007 /* Socket error status */
#define SO_TYPE         0x1008 /* Socket type */
#define SO_NO_CHECK     0x100a /* Don't create UDP checksum */
#define SO_BINDTODEVICE 0x100b /* Bind socket to particular device (e.g. network interface eth0) */


/* Socket shutdown types */
#define SHUT_RD   0 /* Shutdown receptions */
#define SHUT_WR   1 /* Shutdown transmissions */
#define SHUT_RDWR 2 /* Shutdown receptions/transmissions */


/* Address families */
#define AF_UNSPEC 0  /* Unspecified */
#define AF_UNIX   1  /* UNIX domain sockets */
#define AF_INET   2  /* Internet domain sockets for use with IPv4 addresses */
#define AF_INET6  10 /* Internet domain sockets for use with IPv6 addresses */
#define AF_KEY    15 /* Key management API */
#define AF_PACKET 17 /* Packet family */


/* Protocol families */
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6
#define PF_KEY    AF_KEY
#define PF_PACKET AF_PACKET


typedef unsigned char sa_family_t; /* Address family type */
typedef size_t socklen_t;          /* Generic size type (at least 32-bit) */


struct sockaddr {
	sa_family_t sa_family; /* Address family */
	char sa_data[];        /* Socket address */
};


struct sockaddr_storage {
	sa_family_t ss_family;
	char ss_data[128 - sizeof(sa_family_t)];
};


struct linger {
	int l_onoff;  /* Option on/off */
	int l_linger; /* Linger time in seconds */
};


struct msghdr {
	void *msg_name;           /* Optional address */
	socklen_t msg_namelen;    /* Size of address */
	struct iovec *msg_iov;    /* Scatter/gather array */
	int msg_iovlen;           /* Members in msg_iov */
	void *msg_control;        /* Ancillary data */
	socklen_t msg_controllen; /* Ancillary data length */
	int msg_flags;            /* Flags on received message */
};


struct cmsghdr {
	socklen_t cmsg_len; /* Data byte count, including cmsghdr */
	int cmsg_level;     /* Originating protocol */
	int cmsg_type;      /* Protocol specific type */
};


/* Message flags */
#define MSG_PEEK     0x01 /* Leave received data in queue */
#define MSG_TRUNC    0x02 /* Normal data truncated */
#define MSG_CTRUNC   0x04 /* Control data truncated */
#define MSG_WAITALL  0x08 /* Attempt to fill the read buffer */
#define MSG_DONTWAIT 0x10 /* Nonblocking I/O */
#define MSG_OOB      0x20 /* Process out of band data */
#define MSG_MORE     0x40 /* Sender will send more */
#define MSG_NOSIGNAL 0x80 /* Don't send SIGPIPE on send to no longer connected stream socket */


/* Socket level ancillary data types */
#define SCM_RIGHTS 0x01 /* Indicates that the data array contains the access rights to be sent or received */


/* Ancillary data object manipulation macros */
#define CMSG_ALIGN(n) (((n) + sizeof(socklen_t) - 1) & ~(sizeof(socklen_t) - 1))
#define CMSG_SPACE(n) (sizeof(struct cmsghdr) + CMSG_ALIGN(n))
#define CMSG_LEN(n)   (sizeof(struct cmsghdr) + (n))
#define CMSG_DATA(c)  ((unsigned char *)((struct cmsghdr *)(c) + 1))


#define CMSG_FIRSTHDR(m) \
	({ \
		struct msghdr *_m = (struct msghdr *)(m); \
		_m->msg_controllen < sizeof(struct cmsghdr) ? NULL : (struct cmsghdr *)_m->msg_control; \
	})


#define CMSG_NXTHDR(m, c) \
	({ \
		struct msghdr *_m = (struct msghdr *)(m); \
		struct cmsghdr *_c = (struct cmsghdr *)(c); \
		char *_n = (char *)_c + CMSG_SPACE(_c->cmsg_len); \
		char *_e = (char *)_m->msg_control + _m->msg_controllen; \
		(_n > _e ? NULL : (struct cmsghdr *)_n); \
	})


#endif
