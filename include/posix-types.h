/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - types
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_POSIX_TYPES_H_
#define _PH_POSIX_TYPES_H_


typedef int pid_t;
typedef unsigned int mode_t;
typedef int gid_t;
typedef int uid_t;

typedef int dev_t;
typedef unsigned int ino_t; /* FIXME: should be unsigned long long to encode id_t? */
typedef int nlink_t;
typedef int blksize_t;
typedef long long blkcnt_t;
typedef unsigned long long fsblkcnt_t;
typedef unsigned long long fsfilcnt_t;


#endif
