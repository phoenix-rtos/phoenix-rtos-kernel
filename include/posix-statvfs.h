/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - statvfs
 *
 * Copyright 2025 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PHOENIX_POSIX_STATVFS_H_
#define _PHOENIX_POSIX_STATVFS_H_

#include "posix-types.h"


struct statvfs {
	unsigned long f_bsize;   /* Filesystem block size */
	unsigned long f_frsize;  /* Fundamental filesystem block size */
	fsblkcnt_t f_blocks;     /* Number of blocks on filesystem (in f_frsize units) */
	fsblkcnt_t f_bfree;      /* Number of free blocks */
	fsblkcnt_t f_bavail;     /* Number of free blocks available to non-privileged process */
	fsfilcnt_t f_files;      /* Number of inodes */
	fsfilcnt_t f_ffree;      /* Number of free inodes */
	fsfilcnt_t f_favail;     /* Number of free inodes available to non-privileged process */
	unsigned long f_fsid;    /* Filesystem ID */
	unsigned long f_flag;    /* Filesystem flags */
	unsigned long f_namemax; /* Maximum filename length */
};


#endif
