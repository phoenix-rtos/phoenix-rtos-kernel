/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX file status symbolic constants and types
 *
 * Copyright 2018, 2021 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_STAT_H_
#define _PHOENIX_POSIX_STAT_H_

#include "timespec.h"
#include "types.h"


/* File type */
#define S_IFMT   0170000 /* Mask for type */
#define S_IFIFO  0010000 /* FIFO */
#define S_IFCHR  0020000 /* Character device */
#define S_IFDIR  0040000 /* Directory */
#define S_IFBLK  0060000 /* Block device */
#define S_IFREG  0100000 /* Regular file */
#define S_IFLNK  0120000 /* Symbolic link */
#define S_IFSOCK 0140000 /* Socket */


/* File type from mode */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)  /* FIFO */
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)  /* Character device */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)  /* Directory */
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)  /* Block device */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)  /* Regular file */
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)  /* Symbolic link */
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* Socket */


/* File mode bits */
#define S_IRWXU 0000700 /* RWX mask for owner */
#define S_IRUSR 0000400 /* Read permission for owner */
#define S_IWUSR 0000200 /* Write permission for owner */
#define S_IXUSR 0000100 /* Execute/search permission for owner */

#define S_IRWXG 0000070 /* RWX mask for group */
#define S_IRGRP 0000040 /* Read permission for group */
#define S_IWGRP 0000020 /* Write permission for group */
#define S_IXGRP 0000010 /* Execute/search permission for group */

#define S_IRWXO 0000007 /* RWX mask for others */
#define S_IROTH 0000004 /* Read permission for others */
#define S_IWOTH 0000002 /* Write permission for others */
#define S_IXOTH 0000001 /* Execute/search permission for others */

#define S_ISUID 0004000 /* Set user ID on execution */
#define S_ISGID 0002000 /* set group ID on execution */
#define S_ISTXT 0001000 /* Sticky bit */
#define S_ISVTX 0001000 /* Save swapped text even after use */

#define S_IREAD  S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC  S_IXUSR

#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) /* 0666 */


struct stat {
	dev_t st_dev;            /* Device ID containing the file */
	ino_t st_ino;            /* File serial number */
	mode_t st_mode;          /* File mode */
	nlink_t st_nlink;        /* Number of hard links to the file */
	uid_t st_uid;            /* File user ID */
	gid_t st_gid;            /* File group ID */
	dev_t st_rdev;           /* Device ID (for character/block device file) */
	off_t st_size;           /* Regular file size (pathname length for symbolic links) */
	struct timespec st_atim; /* Last data access timestamp */
	struct timespec st_mtim; /* Last data modification timestamp */
	struct timespec st_ctim; /* Last file status change timestamp */
	blksize_t st_blksize;    /* Preferred block size for the file */
	blkcnt_t st_blocks;      /* Number of blocks allocated for the file */
};


/* Macros for struct stat backward compatibility */
#define st_atime st_atim.tv_sec
#define st_ctime st_ctim.tv_sec
#define st_mtime st_mtim.tv_sec


#endif
