/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX fcntl() symbolic constants and types
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_FCNTL_H_
#define _PHOENIX_POSIX_FCNTL_H_

#include "types.h"


/* File descriptor flags */
#define FD_CLOEXEC 1 /* Close on exec */


/* File access modes and status flags */
#define O_RDONLY   0x0001 /* Read access only */
#define O_WRONLY   0x0002 /* Write access only */
#define O_RDWR     0x0004 /* Both read and write access */
#define O_APPEND   0x0008 /* Write past end of the file */
#define O_CREAT    0x0100 /* Create the file if it doesn't exist */
#define O_TRUNC    0x0200 /* Wipe out the file */
#define O_EXCL     0x0400 /* With O_CREAT, will fail if the file exists */
#define O_SYNC     0x0800 /* Synchronize write operations with the hardware */
#define O_NONBLOCK 0x1000 /* Use nonblocking I/O */
#define O_NOCTTY   0x2000 /* Don't set as controlling terminal (for terminal device files) */
#define O_CLOEXEC  0x4000 /* Set FD_CLOEXEC flag */
#define O_NDELAY O_NONBLOCK


/* fcntl() operations */
enum {
	F_DUPFD,         /* Duplicate file descriptor */
	F_DUPFD_CLOEXEC, /* Same as F_DUPFD but additionaly sets FD_CLOEXEC flag */
	F_GETFD,         /* Get file descriptor flags */
	F_SETFD,         /* Set file descriptor flags */
	F_GETFL,         /* Get file access mode and status flags */
	F_SETFL,         /* Set file access mode and status flags */
	F_GETOWN,        /* Get process (group) ID receiving SIGIO and SIGURG signals */
	F_SETOWN,        /* Set process (group) ID receiving SIGIO and SIGURG signals */
	F_GETLK,         /* Get incompatible lock info for placing a new file lock */
	F_SETLK,         /* Acquire (F_RDLCK or F_WRLCK) / release (F_UNLCK) a file lock */
	F_SETLKW         /* Same as F_SETLK but waits for conflicting lock to be released */
};


/* Open file description locks */
enum {
	LOCK_SH = 1, /* Shared lock */
	LOCK_EX = 2, /* Exclusive lock */
	LOCK_NB = 4, /* Prevents blocking, used with above flags */
	LOCK_UN = 8  /* Remove lock */
};


/* Lock types */
enum {
	F_RDLCK, /* Read lock */
	F_WRLCK, /* Write lock */
	F_UNLCK  /* Release lock */
};


/* Lock description */
struct flock {
	short l_type;
	short l_whence;
	off_t l_start;
	off_t l_len;
	pid_t l_pid;
};


#endif
