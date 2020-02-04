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


struct _msg_t;


typedef struct _hades_t hades_t;

typedef struct _iodes_t iodes_t;


typedef struct _obdes_t {
	rbnode_t linkage;
	id_t id;
	int refs;
	wait_note_t *queue;
	struct _port_t *port;

	union {
		struct _pipe_t *pipe;
	};
} obdes_t;


enum { ftRegular, ftDirectory, ftDevice, ftPort, ftFifo, ftPipe, ftLocalSocket, ftSocket };


struct _iodes_t {
	int refs;
	lock_t lock;
	unsigned status;
	off_t offset;
	int type;

	struct _obdes_t *obdes;

	struct {
		struct _port_t *port;
		id_t id;
	} fs;

	union {
		struct _port_t *port;
		struct _pipe_t *pipe;
		struct _sun_t *sun;

		struct {
			struct _port_t *port;
			id_t id;
		} device;
	};
};


#define HD_TYPE(flags) (((flags) >> 8) & 0xff)
#define HD_MESSAGE (hades_msg << 8)


enum { hades_io = 0, hades_msg };


struct _hades_t {
	union {
		iodes_t *file;
		struct _kmsg_t *msg;
	};

	int flags;
};


typedef struct stat file_stat_t;


extern iodes_t *file_alloc(void);


extern int fd_new(struct _process_t *p, int handle, int flags, iodes_t *file);


extern iodes_t *file_get(struct _process_t *p, int fd);


extern void file_ref(iodes_t *f);


extern int file_put(iodes_t *f);


extern int file_open(iodes_t **result, struct _process_t *process, int dirfd, const char *path, int flags, mode_t mode);


extern ssize_t file_read(iodes_t *file, void *data, size_t size, off_t offset);


extern int file_resolve(iodes_t **result, struct _process_t *process, int handle, const char *path, int flags);


//extern int fd_create(struct _process_t *p, int minfd, int flags, unsigned int status, const file_ops_t *ops, void *data);


extern int fd_close(struct _process_t *p, int fd);


extern int proc_changeDir(int handle, const char *path);


extern int proc_queueCreate(void);


extern int proc_pipeCreate(int fds[2], int flags);


extern int proc_fifoCreate(int dirfd, const char *path, mode_t mode);

extern int proc_deviceCreate(int dirfd, const char *path, int portfd, id_t id, mode_t mode);


extern int proc_queueWait(int fd, const event_t *subs, int subcnt, event_t *events, int evcnt, time_t timeout);


extern int proc_fileResolve(struct _process_t *process, int handle, const char *path, int flags, oid_t *oid);


extern int proc_filesCopy(struct _process_t *parent);


extern int proc_filesCloseExec(struct _process_t *process);


extern int proc_fileOid(struct _process_t *process, int fd, oid_t *oid);


extern int proc_fileOpen(int dirfd, const char *path, int flags, mode_t mode);


extern int proc_fileClose(int handle);


extern ssize_t proc_fileRead(int handle, char *buf, size_t nbyte, off_t *offset);


extern ssize_t proc_fileWrite(int handle, const char *buf, size_t nbyte, off_t *offset);


extern int proc_fileDup(int handle, int fildes2, int flags);


extern int proc_fileLink(int handle, const char *path, int dirfd, const char *name, int flags);


extern int proc_fileUnlink(int dirfd, const char *path, int flags);


extern int proc_fileSeek(int handle, off_t *offset, int whence);


extern int proc_fileTruncate(int handle, off_t length);


extern int proc_fileControl(int handle, int cmd, long arg);


extern int proc_fileStat(int handle, const char *path, file_stat_t *buf, int flags);


extern int proc_fileChmod(int handle, mode_t mode);


extern int proc_fileIoctl(int handle, unsigned long request, const char *indata, size_t insz, char *outdata, size_t outsz);


extern int proc_filesDestroy(struct _process_t *process);


extern int proc_filesSetRoot(int port, id_t id, mode_t mode);


extern int proc_fsMount(int devfd, const char *devpath, const char *type, unsigned port);


extern int proc_fsBind(int dirfd, const char *dirpath, int fsfd, const char *fspath);


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


extern int proc_msgRespond(int porth, int handle, int error, struct _msg_t *msg);


extern int proc_msgSend(int handle, struct _msg_t *msg);


extern int proc_msgRecv(int handle, struct _msg_t *msg);


extern void _file_init(void);

#endif
