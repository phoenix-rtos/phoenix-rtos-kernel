/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Files
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_FILE_H_
#define _PROC_FILE_H_

#include HAL
#include "../../include/types.h"
#include "../../include/socket.h"
#include "../../include/stat.h"
#include "process.h"
#include "event.h"

typedef struct _fildes_t fildes_t;


typedef struct _file_t file_t;


typedef struct _file_ops_t {
	ssize_t (*read)(file_t *, void *, size_t);
	ssize_t (*write)(file_t *, const void *, size_t);
	int (*release)(file_t *);
	int (*seek)(file_t *, off_t *, int);
	ssize_t (*setattr)(file_t *, int, const void *, size_t);
	ssize_t (*getattr)(file_t *, int, void *, size_t);
	int (*ioctl)(file_t *, unsigned, const void *, size_t, void *, size_t);
	int (*link)(file_t *, const char *, const oid_t *);
	int (*unlink)(file_t *, const char *);
} file_ops_t;


struct _file_t {
	unsigned refs;
	off_t offset;
	lock_t lock;
	mode_t mode;
	unsigned status;
	const file_ops_t *ops;
	oid_t oid;

	union {
		struct _pipe_t *pipe;
		struct _named_pipe_t *npipe;
		struct _evqueue_t *queue;
		struct _port_t *port;
	};
};


struct _fildes_t {
	file_t *file;
	unsigned flags;
};


typedef struct stat file_stat_t;


extern int proc_queueCreate(void);


extern int proc_pipeCreate(int fds[2]);


extern int proc_fifoCreate(int dirfd, const char *path, mode_t mode);


extern int proc_queueWait(int fd, const event_t *subs, int subcnt, event_t *events, int evcnt, time_t timeout);


extern int proc_fileResolve(struct _process_t *process, int fildes, const char *path, int flags, oid_t *oid);


extern int proc_filesCopy(struct _process_t *parent);


extern int proc_filesCloseExec(struct _process_t *process);


extern int proc_fileOid(struct _process_t *process, int fd, oid_t *oid);


extern int proc_fileOpen(int dirfd, const char *path, int flags, mode_t mode);


extern int proc_fileClose(int fildes);


extern ssize_t proc_fileRead(int fildes, char *buf, size_t nbyte);


extern ssize_t proc_fileWrite(int fildes, const char *buf, size_t nbyte);


extern int proc_fileDup(int fildes, int fildes2, int flags);


extern int proc_fileLink(int fildes, const char *path, int dirfd, const char *name, int flags);


extern int proc_fileUnlink(int dirfd, const char *path, int flags);


extern int proc_fileSeek(int fildes, off_t *offset, int whence);


extern int proc_fileTruncate(int fildes, off_t length);


extern int proc_fileControl(int fildes, int cmd, long arg);


extern int proc_fileStat(int fildes, const char *path, file_stat_t *buf, int flags);


extern int proc_fileChmod(int fildes, mode_t mode);


extern int proc_fileIoctl(int fildes, unsigned long request, const char *indata, size_t insz, char *outdata, size_t outsz);


extern int proc_filesDestroy(struct _process_t *process);


extern int proc_filesSetRoot(const oid_t *oid, mode_t mode);


extern int proc_netAccept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags);


extern int proc_netBind(int socket, const struct sockaddr *address, socklen_t address_len);


extern int proc_netConnect(int socket, const struct sockaddr *address, socklen_t address_len);


extern int proc_netGetpeername(int socket, struct sockaddr *address, socklen_t *address_len);


extern int proc_netGetsockname(int socket, struct sockaddr *address, socklen_t *address_len);


extern int proc_netGetsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen);


extern int proc_netListen(int socket, int backlog);


extern ssize_t proc_netRecvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t proc_netSendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern int proc_netShutdown(int socket, int how);


extern int proc_netSetsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);


extern int proc_netSocket(int domain, int type, int protocol);


extern void _file_init(void);

#endif
