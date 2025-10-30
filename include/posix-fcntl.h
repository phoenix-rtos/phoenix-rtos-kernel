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

#define O_RDONLY   0x00001
#define O_WRONLY   0x00002
#define O_RDWR     0x00004
#define O_APPEND   0x00008
#define O_CREAT    0x00100
#define O_TRUNC    0x00200
#define O_EXCL     0x00400
#define O_SYNC     0x00800
#define O_NONBLOCK 0x01000
#define O_NDELAY   O_NONBLOCK
#define O_NOCTTY   0x02000
#define O_CLOEXEC  0x04000
#define O_RSYNC    0x08000
#define O_DSYNC    0x10000

#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)

/* clang-format off */

/* fcntl() operations */
enum { F_DUPFD = 0, F_DUPFD_CLOEXEC, F_GETFD, F_SETFD, F_GETFL, F_SETFL,
	F_GETOWN, F_SETOWN, F_GETLK, F_SETLK, F_SETLKW };

/* clang-format on */


#endif
