/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * fcntl.h
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_FCNTL_H_
#define _PHOENIX_FCNTL_H_

#include "types.h"

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR 0x0004
#define O_APPEND 0x0008
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_EXCL 0x0400
#define O_SYNC 0x0800
#define O_NONBLOCK 0x1000
#define O_NDELAY O_NONBLOCK
#define O_NOCTTY 0x2000
#define O_NOFOLLOW 0x4000
#define O_DIRECTORY 0x8000
#define O_PARENT 0x10000


enum { F_DUPFD, F_DUPFD_CLOEXEC, F_GETFD, F_SETFD, F_GETFL, F_SETFL,
       F_GETOWN, F_SETOWN, F_GETLK, F_SETLK, F_SETLKW };


#define FD_CLOEXEC 1
#define FD_ALLOC 2

#define AT_FDCWD (-100)
#define AT_FDSYSPAGE (-101)

enum {
	LOCK_SH = 1,
	LOCK_EX = 2,
	LOCK_NB = 4,
	LOCK_UN = 8,
};


enum { F_RDLCK, F_UNLCK, F_WRLCK };


struct flock {
	short l_type;
	short l_whence;
	off_t l_start;
	off_t l_len;
	pid_t l_pid;
};


#endif
