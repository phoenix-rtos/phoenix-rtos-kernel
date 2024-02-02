/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - fcntl
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_FCNTL_H_
#define _PHOENIX_POSIX_FCNTL_H_


#define FD_CLOEXEC 1

#define O_RDONLY   0x0001
#define O_WRONLY   0x0002
#define O_RDWR     0x0004
#define O_APPEND   0x0008
#define O_CREAT    0x0100
#define O_TRUNC    0x0200
#define O_EXCL     0x0400
#define O_SYNC     0x0800
#define O_NONBLOCK 0x1000
#define O_NDELAY   O_NONBLOCK
#define O_NOCTTY   0x2000
#define O_CLOEXEC  0x4000

/* clang-format off */

/* fcntl() operations */
enum { F_DUPFD = 0, F_DUPFD_CLOEXEC, F_GETFD, F_SETFD, F_GETFL, F_SETFL,
	F_GETOWN, F_SETOWN, F_GETLK, F_SETLK, F_SETLKW };

/* clang-format on */


#endif
