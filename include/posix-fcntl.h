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

#ifndef _PH_POSIX_FCNTL_H_
#define _PH_POSIX_FCNTL_H_


#define FD_CLOEXEC 0x1U

/*
 * These are enum values, not a bitfields, because POSIX requires access modes
 * to be mutually exclusive. It does not specify the format or values for those
 * definitions, however, most POSIX conformant operating systems treat those
 * values as enumerations.
 */
#define O_RDONLY 0x00000U
#define O_WRONLY 0x00001U
#define O_RDWR   0x00002U

#define O_ACCMODE 0x00003U

#define O_APPEND   0x00008U
#define O_CREAT    0x00100U
#define O_TRUNC    0x00200U
#define O_EXCL     0x00400U
#define O_SYNC     0x00800U
#define O_NONBLOCK 0x01000U
#define O_NDELAY   O_NONBLOCK
#define O_NOCTTY   0x02000U
#define O_CLOEXEC  0x04000U
#define O_RSYNC    0x08000U
#define O_DSYNC    0x10000U

/* clang-format off */

/* fcntl() operations */
enum { F_DUPFD = 0, F_DUPFD_CLOEXEC, F_GETFD, F_SETFD, F_GETFL, F_SETFL,
	F_GETOWN, F_SETOWN, F_GETLK, F_SETLK, F_SETLKW };

/* clang-format on */


#endif
