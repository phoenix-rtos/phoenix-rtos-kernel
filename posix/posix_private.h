/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Private
 *
 * Copyright 2021 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_POSIX_POSIX_PRIVATE_H_
#define _PH_POSIX_POSIX_PRIVATE_H_

#include "hal/hal.h"
#include "proc/proc.h"
#include "posix.h"

/* This define is used against oid_t.id which is __u32
 * hence the implicit bit value instead of -1
 */
#define US_PORT 0xffffffffU /* FIXME */


#define HOST_NAME_MAX 255U


enum { ftRegular,
	ftPipe,
	ftFifo,
	ftInetSocket,
	ftUnixSocket,
	ftTty };


/* FIXME: share with posixsrv */
enum { pxBufferedPipe,
	pxPipe,
	pxPTY };


#define F_SEEKABLE(type) ((type) == ftRegular)


typedef struct {
	oid_t ln;
	oid_t oid;
	int refs;
	off_t offset;
	unsigned int status;
	lock_t lock;
	int type;
} open_file_t;


typedef struct {
	open_file_t *file;
	unsigned int flags;
} fildes_t;


typedef struct _process_info_t {
	rbnode_t linkage;
	int process;
	int parent;
	int refs;
	int exitcode;

	thread_t *wait;

	struct _process_info_t *children;
	struct _process_info_t *zombies;
	struct _process_info_t *next, *prev;

	pid_t pgid;
	lock_t lock;
	int maxfd;
	int fdsz;
	fildes_t *fds;
} process_info_t;


/* SIOCGIFCONF ioctl special case: arg is structure with pointer */
struct ifconf {
	unsigned int ifc_len; /* size of buffer */
	char *ifc_buf;        /* buffer address */
};

/* SIOADDRT and SIOCDELRT ioctls special case: arg is structure with pointer */
struct rtentry {
	struct sockaddr rt_dst;
	struct sockaddr rt_gateway;
	struct sockaddr rt_genmask;
	short rt_flags;
	short rt_metric;
	char *rt_dev;
	unsigned long rt_mss;
	unsigned long rt_window;
	unsigned short rt_irtt;
};


int posix_fileDeref(open_file_t *f);


int posix_getOpenFile(int fd, open_file_t **f);


int posix_newFile(process_info_t *p, int fd);


int _posix_addOpenFile(process_info_t *p, open_file_t *f, unsigned int flags);


process_info_t *pinfo_find(int pid);


void pinfo_put(process_info_t *p);


int inet_accept4(unsigned int socket, struct sockaddr *address, socklen_t *address_len, unsigned int flags);


int inet_bind(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


int inet_connect(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


int inet_getpeername(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


int inet_getsockname(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


int inet_getsockopt(unsigned int socket, int level, int optname, void *optval, socklen_t *optlen);


int inet_listen(unsigned int socket, int backlog);


ssize_t inet_recvfrom(unsigned int socket, void *message, size_t length, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len);


ssize_t inet_sendto(unsigned int socket, const void *message, size_t length, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


ssize_t inet_recvmsg(unsigned int socket, struct msghdr *msg, unsigned int flags);


ssize_t inet_sendmsg(unsigned int socket, const struct msghdr *msg, unsigned int flags);


int inet_socket(int domain, int type, int protocol);


int inet_shutdown(unsigned int socket, int how);


int inet_setsockopt(unsigned int socket, int level, int optname, const void *optval, socklen_t optlen);


int inet_setfl(unsigned int socket, unsigned int flags);


int inet_getfl(unsigned int socket);


int unix_accept4(unsigned int socket, struct sockaddr *address, socklen_t *address_len, unsigned int flags);


int unix_bind(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


int unix_connect(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


int unix_getpeername(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


int unix_getsockname(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


int unix_getsockopt(unsigned int socket, int level, int optname, void *optval, socklen_t *optlen);


int unix_listen(unsigned int socket, int backlog);


ssize_t unix_recvfrom(unsigned int socket, void *msg, size_t len, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len);


ssize_t unix_sendto(unsigned int socket, const void *msg, size_t len, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


ssize_t unix_recvmsg(unsigned int socket, struct msghdr *msg, unsigned int flags);


ssize_t unix_sendmsg(unsigned int socket, const struct msghdr *msg, unsigned int flags);


int unix_socket(int domain, unsigned int type, int protocol);


int unix_socketpair(int domain, unsigned int type, int protocol, int sv[2]);


int unix_shutdown(unsigned int socket, int how);


int unix_unlink(unsigned int socket);


int unix_setsockopt(unsigned int socket, int level, int optname, const void *optval, socklen_t optlen);


int unix_setfl(unsigned int socket, unsigned int flags);


int unix_getfl(unsigned int socket);


int unix_close(unsigned int socket);


int unix_poll(unsigned int socket, unsigned short events);


void unix_sockets_init(void);

#endif
