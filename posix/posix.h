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
#include "include/utsname.h"
#include "include/posix-fcntl.h"
#include "include/posix-poll.h"
#include "include/posix-socket.h"
#include "include/posix-stat.h"
#include "include/posix-statvfs.h"
#include "include/posix-stdio.h"
#include "include/posix-timespec.h"
#include "include/posix-uio.h"
#include "sockport.h"


int posix_open(const char *filename, int oflag, u8 *ustack);


int posix_close(int fildes);


ssize_t posix_read(int fildes, void *buf, size_t nbyte, off_t offset);


ssize_t posix_write(int fildes, void *buf, size_t nbyte, off_t offset);


int posix_getOid(int fildes, oid_t *oid);


int posix_dup(int fildes);


int posix_dup2(int fildes, int fildes2);


int posix_link(const char *path1, const char *path2);


int posix_unlink(const char *pathname);


int posix_lseek(int fildes, off_t *offset, int whence);


int posix_ftruncate(int fildes, off_t length);


int posix_fcntl(int fd, unsigned int cmd, u8 *ustack);


int posix_pipe(int fildes[2]);


int posix_mkfifo(const char *pathname, mode_t mode);


int posix_chmod(const char *pathname, mode_t mode);


int posix_fstat(int fd, struct stat *buf);


int posix_statvfs(const char *path, int fildes, struct statvfs *buf);


int posix_fsync(int fd);


int posix_clone(int ppid);


int posix_exec(void);


int posix_accept(int socket, struct sockaddr *address, socklen_t *address_len);


int posix_accept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags);


int posix_bind(int socket, const struct sockaddr *address, socklen_t address_len);


int posix_connect(int socket, const struct sockaddr *address, socklen_t address_len);


int posix_gethostname(char *name, size_t namelen);


int posix_uname(struct utsname *name);


int posix_getpeername(int socket, struct sockaddr *address, socklen_t *address_len);


int posix_getsockname(int socket, struct sockaddr *address, socklen_t *address_len);


int posix_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen);


int posix_listen(int socket, int backlog);


ssize_t posix_recvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);


ssize_t posix_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


ssize_t posix_recvmsg(int socket, struct msghdr *msg, int flags);


ssize_t posix_sendmsg(int socket, const struct msghdr *msg, int flags);


int posix_socket(int domain, int type, int protocol);


int posix_socketpair(int domain, int type, int protocol, int sv[2]);


int posix_shutdown(int socket, int how);


int posix_sethostname(const char *name, size_t namelen);


int posix_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);


int posix_ioctl(int fildes, unsigned long request, u8 *ustack);


int posix_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms);


int posix_futimens(int fildes, const struct timespec *times);


int posix_tkill(pid_t pid, int tid, int sig);


void posix_sigchild(pid_t ppid);


int posix_setpgid(pid_t pid, pid_t pgid);


pid_t posix_getpgid(pid_t pid);


pid_t posix_getppid(pid_t pid);


pid_t posix_setsid(void);


void posix_died(pid_t pid, int exit);


int posix_waitpid(pid_t child, int *status, unsigned int options);


void posix_init(void);


#endif
