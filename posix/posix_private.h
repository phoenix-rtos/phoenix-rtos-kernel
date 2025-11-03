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

#ifndef _POSIX_POSIX_PRIVATE_H_
#define _POSIX_POSIX_PRIVATE_H_

#include "hal/hal.h"
#include "proc/proc.h"
#include "posix.h"

/* This define is used against oid_t.id which is __u32
 * hence the implicit bit value instead of -1
 */
#define US_PORT 0xFFFFFFFFU /* FIXME */


#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    SIGABRT
#define SIGEMT    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGBUS    10
#define SIGSEGV   11
#define SIGSYS    12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGURG    16
#define SIGSTOP   17
#define SIGTSTP   18
#define SIGCONT   19
#define SIGCHLD   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGIO     23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGINFO   29
#define SIGUSR1   30
#define SIGUSR2   31

#define NSIG 32

#define SIG_ERR (-1)
#define SIG_DFL (-2)
#define SIG_IGN (-3)

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
	unsigned int refs;
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


extern int posix_fileDeref(open_file_t *f);


extern int posix_getOpenFile(int fd, open_file_t **f);


extern int posix_newFile(process_info_t *p, int fd);


extern int _posix_addOpenFile(process_info_t *p, open_file_t *f, unsigned int flags);


extern process_info_t *pinfo_find(int pid);


extern void pinfo_put(process_info_t *p);


extern int inet_accept4(unsigned int socket, struct sockaddr *address, socklen_t *address_len, unsigned int flags);


extern int inet_bind(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


extern int inet_connect(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


extern int inet_getpeername(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


extern int inet_getsockname(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


extern int inet_getsockopt(unsigned int socket, int level, int optname, void *optval, socklen_t *optlen);


extern int inet_listen(unsigned int socket, int backlog);


extern ssize_t inet_recvfrom(unsigned int socket, void *message, size_t length, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t inet_sendto(unsigned int socket, const void *message, size_t length, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern ssize_t inet_recvmsg(unsigned int socket, struct msghdr *msg, unsigned int flags);


extern ssize_t inet_sendmsg(unsigned int socket, const struct msghdr *msg, unsigned int flags);


extern int inet_socket(int domain, int type, int protocol);


extern int inet_shutdown(unsigned int socket, int how);


extern int inet_setsockopt(unsigned int socket, int level, int optname, const void *optval, socklen_t optlen);


extern int inet_setfl(unsigned int socket, int flags);


extern int inet_getfl(unsigned int socket);


extern int unix_accept4(unsigned int socket, struct sockaddr *address, socklen_t *address_len, unsigned int flags);


extern int unix_bind(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


extern int unix_connect(unsigned int socket, const struct sockaddr *address, socklen_t address_len);


extern int unix_getpeername(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


extern int unix_getsockname(unsigned int socket, struct sockaddr *address, socklen_t *address_len);


extern int unix_getsockopt(unsigned int socket, int level, int optname, void *optval, socklen_t *optlen);


extern int unix_listen(unsigned int socket, int backlog);


extern ssize_t unix_recvfrom(unsigned int socket, void *msg, size_t len, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t unix_sendto(unsigned int socket, const void *msg, size_t len, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern ssize_t unix_recvmsg(unsigned int socket, struct msghdr *msg, unsigned int flags);


extern ssize_t unix_sendmsg(unsigned int socket, const struct msghdr *msg, unsigned int flags);


extern int unix_socket(int domain, int type, int protocol);


extern int unix_socketpair(int domain, int type, int protocol, int sv[2]);


extern int unix_shutdown(unsigned int socket, int how);


extern int unix_unlink(unsigned int socket);


extern int unix_setsockopt(unsigned int socket, int level, int optname, const void *optval, socklen_t optlen);


extern int unix_setfl(unsigned int socket, unsigned int flags);


extern int unix_getfl(unsigned int socket);


extern int unix_close(unsigned int socket);


extern int unix_poll(unsigned int socket, unsigned short events);


extern void unix_sockets_init(void);

#endif
