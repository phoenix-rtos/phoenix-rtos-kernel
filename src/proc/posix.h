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

typedef int off_t;
typedef int mode_t;


extern int posix_open(const char *filename, int oflag, char *ustack);


extern int posix_close(int fildes);


extern int posix_read(int fildes, void *buf, size_t nbyte);


extern int posix_write(int fildes, void *buf, size_t nbyte);


extern int posix_dup(int fildes);


extern int posix_dup2(int fildes, int fildes2);


extern int posix_link(const char *path1, const char *path2);


extern int posix_unlink(const char *pathname);


extern off_t posix_lseek(int fildes, off_t offset, int whence);


extern int posix_ftruncate(int fildes, off_t length);


extern int posix_fcntl(unsigned int fd, unsigned int cmd, char *ustack);


extern int posix_fork(void);


extern int posix_pipe(int fildes[2]);


extern int posix_clone(int ppid);


extern int posix_exit(process_t *process);


extern void posix_init(void);

#endif
