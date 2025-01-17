/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PROC_POSIX_H_
#define _PROC_POSIX_H_


#include "include/types.h"
#include "include/posix-fcntl.h"
#include "include/posix-poll.h"
#include "include/posix-socket.h"
#include "include/posix-stat.h"
#include "include/posix-statvfs.h"
#include "include/posix-stdio.h"
#include "include/posix-timespec.h"
#include "include/posix-uio.h"
#include "sockport.h"


extern int posix_open(const char *filename, int oflag, char *ustack);


extern int posix_close(int fildes);


extern ssize_t posix_read(int fildes, void *buf, size_t nbyte);


extern ssize_t posix_write(int fildes, void *buf, size_t nbyte);


extern int posix_getOid(int fildes, oid_t *oid);


extern int posix_dup(int fildes);


extern int posix_dup2(int fildes, int fildes2);


extern int posix_link(const char *path1, const char *path2);


extern int posix_unlink(const char *pathname);


extern int posix_lseek(int fildes, off_t *offset, int whence);


extern int posix_ftruncate(int fildes, off_t length);


extern int posix_fcntl(int fd, unsigned int cmd, char *ustack);


extern int posix_pipe(int fildes[2]);


extern int posix_mkfifo(const char *path, mode_t mode);


extern int posix_chmod(const char *path, mode_t mode);


extern int posix_fstat(int fd, struct stat *buf);


extern int posix_statvfs(const char *path, int fd, struct statvfs *buf);


extern int posix_fsync(int fd);


extern int posix_clone(int ppid);


extern int posix_exec(void);


extern int posix_accept(int socket, struct sockaddr *address, socklen_t *address_len);


extern int posix_accept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags);


extern int posix_bind(int socket, const struct sockaddr *address, socklen_t address_len);


extern int posix_connect(int socket, const struct sockaddr *address, socklen_t address_len);


extern int posix_gethostname(char *name, size_t namelen);


extern int posix_getpeername(int socket, struct sockaddr *address, socklen_t *address_len);


extern int posix_getsockname(int socket, struct sockaddr *address, socklen_t *address_len);


extern int posix_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen);


extern int posix_listen(int socket, int backlog);


extern ssize_t posix_recvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t posix_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern ssize_t posix_recvmsg(int socket, struct msghdr *msg, int flags);


extern ssize_t posix_sendmsg(int socket, const struct msghdr *msg, int flags);


extern int posix_socket(int domain, int type, int protocol);


extern int posix_socketpair(int domain, int type, int protocol, int sv[2]);


extern int posix_shutdown(int socket, int how);


extern int posix_sethostname(const char *name, size_t namelen);


extern int posix_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);


extern int posix_ioctl(int fildes, unsigned long request, char *ustack);


extern int posix_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms);


extern int posix_futimens(int fildes, const struct timespec *times);


extern int posix_tkill(pid_t pid, int tid, int sig);


extern void posix_sigchild(pid_t ppid);


extern int posix_setpgid(pid_t pid, pid_t pgid);


extern pid_t posix_getpgid(pid_t pid);


extern pid_t posix_getppid(pid_t pid);


extern pid_t posix_setsid(void);


extern void posix_died(pid_t pid, int exit);


extern int posix_waitpid(pid_t child, int *status, int options);


extern void posix_init(void);


#endif
