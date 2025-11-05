/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - stat
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_POSIX_STAT_H_
#define _PH_POSIX_STAT_H_


#include "types.h"
#include "posix-timespec.h"


/* File type */
#define S_IFMT   0xf000U /* File type mask */
#define S_IFSOCK 0xc000U /* Socket */
#define S_IFLNK  0xa000U /* Symbolic link */
#define S_IFREG  0x8000U /* Regular file */
#define S_IFBLK  0x6000U /* Block device */
#define S_IFDIR  0x4000U /* Directory */
#define S_IFCHR  0x2000U /* Character device */
#define S_IFIFO  0x1000U /* FIFO */

#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* Socket */
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)  /* Symbolic link */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)  /* Regular file */
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)  /* Block device */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)  /* Directory */
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)  /* Character device */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)  /* FIFO */


/* File permissions */
#define S_ISUID 0x0800U /* Set user ID on execution */
#define S_ISGID 0x0400U /* Set group ID on execution */
#define S_ISVTX 0x0200U /* Sticky bit */

#define S_IRWXU 0x01c0U /* RWX mask for owner */
#define S_IRUSR 0x0100U /* R for owner */
#define S_IWUSR 0x0080U /* W for owner */
#define S_IXUSR 0x0040U /* X for owner */

#define S_IRWXG 0x0038U /* RWX mask for group */
#define S_IRGRP 0x0020U /* R for group */
#define S_IWGRP 0x0010U /* W for group */
#define S_IXGRP 0x0008U /* X for group */

#define S_IRWXO 0x0007U /* RWX mask for other */
#define S_IROTH 0x0004U /* R for other */
#define S_IWOTH 0x0002U /* W for other */
#define S_IXOTH 0x0001U /* X for other */


/* BSD compatibility macros */
#define S_ISTXT  S_ISVTX
#define S_IREAD  S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC  S_IXUSR

#define S_BLKSIZE 512 /* Block size (stat.st_blocks unit) */

#define ALLPERMS    (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO) /* 07777 */
#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)                               /* 0777 */
#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) /* 0666 */


struct stat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	blksize_t st_blksize;
	blkcnt_t st_blocks;
};


#endif
