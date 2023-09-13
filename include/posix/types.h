/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX system types
 *
 * Copyright 2018, 2021 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_TYPES_H_
#define _PHOENIX_POSIX_TYPES_H_

#include "../types.h"


/* Identifier types (generic ID type id_t is defined in "../types.h") */
typedef int pid_t;     /* Process (group) ID */
typedef int gid_t;     /* Group ID */
typedef int uid_t;     /* User ID */
typedef int dev_t;     /* Device ID */
typedef int key_t;     /* IPC key ID */
typedef int timer_t;   /* Timer ID */
typedef int clockid_t; /* Clock ID */


/* Time related types (generic system time type time_t is defined in "../types.h") */
typedef unsigned int useconds_t; /* Stores time in microseconds */
typedef int suseconds_t;         /* Same as above but signed */
typedef int clock_t;             /* Stores system time in clock ticks or CLOCKS_PER_SEC */


/* Storage and filesystem related types */
typedef long blksize_t;                /* Block size */
typedef long long blkcnt_t;            /* Blocks count */
typedef unsigned long long fsblkcnt_t; /* Filesystem blocks count */
typedef unsigned long long fsfilcnt_t; /* Filesystem files count */


/* File related types (file size type off_t is defined as system type in "../types.h") */
typedef unsigned long long ino_t; /* File serial number (should contain id_t) */
typedef int mode_t;               /* File attributes */
typedef int nlink_t;              /* File links count */
typedef long long off64_t;        /* File size (64-bit) */


#endif
