/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * stat.h
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski, Kamil Amanowicz, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_STAT_H_
#define _PHOENIX_STAT_H_

#include "types.h"


#define S_ISUID 0004000   /* set user id on execution */
#define S_ISGID 0002000   /* set group id on execution */
#define S_ISTXT 0001000   /* sticky bit */

#define S_IRWXU 0000700   /* RWX mask for owner */
#define S_IRUSR 0000400   /* R for owner */
#define S_IWUSR 0000200   /* W for owner */
#define S_IXUSR 0000100   /* X for owner */

#define S_IREAD   S_IRUSR
#define S_IWRITE  S_IWUSR
#define S_IEXEC   S_IXUSR

#define S_IRWXG 0000070   /* RWX mask for group */
#define S_IRGRP 0000040   /* R for group */
#define S_IWGRP 0000020   /* W for group */
#define S_IXGRP 0000010   /* X for group */

#define S_IRWXO 0000007   /* RWX mask for other */
#define S_IROTH 0000004   /* R for other */
#define S_IWOTH 0000002   /* W for other */
#define S_IXOTH 0000001   /* X for other */

#define S_IFMT   0xF000   /* mask for type */
#define S_IFPORT 0xE000   /* port */
#define S_IFMNT  0xD000   /* mount point */
#define S_IFSOCK 0xC000   /* socket */
#define S_IFLNK  0xA000   /* symbolic link */
#define S_IFREG  0x8000   /* regular file */
#define S_IFBLK  0x6000   /* block device */
#define S_IFDIR  0x4000   /* directory */
#define S_IFCHR  0x2000   /* character device */
#define S_IFIFO  0x1000   /* fifo */

#define S_ISVTX  0001000  /* save swapped text even after use */

#define S_BLKSIZE  512  /* block size used in the stat struct */

/* 0666 */
#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)  /* directory */
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)  /* char special */
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)  /* block special */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)  /* regular file */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)  /* fifo */
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)  /* symbolic link */
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* socket */
#define S_ISMNT(m)  (((m) & S_IFMT) == S_IFMNT)   /* mount point */
#define S_ISPORT(m)  (((m) & S_IFMT) == S_IFPORT)   /* port */


typedef int dev_t;


struct stat {
	dev_t     st_dev;
	ino_t     st_ino;
	mode_t    st_mode;
	nlink_t   st_nlink;
	uid_t     st_uid;
	gid_t     st_gid;
	dev_t     st_rdev;
	id_t      st_devid;
	off_t     st_size;
	time_t    st_atime;
	time_t    st_mtime;
	time_t    st_ctime;
	blksize_t st_blksize;
	blkcnt_t  st_blocks;
};

#endif
