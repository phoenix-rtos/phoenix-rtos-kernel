/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_H_
#define _PHOENIX_POSIX_H_

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  5
#define F_SETLK  6
#define F_SETLKW 7

#define F_SETOWN 8
#define F_GETOWN 9
#define F_DUPFD_CLOEXEC  10

#define FD_CLOEXEC 1

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR 0x0004
#define O_APPEND 0x0008
#define O_CREAT 0x0100
#define O_TRUNC 0x0200
#define O_EXCL 0x0400
#define O_SYNC 0x0800
#define O_NONBLOCK 0x1000
#define O_NOCTTY 0x2000
#define O_CLOEXEC 0x4000

typedef int ssize_t;

typedef size_t socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr {
	sa_family_t sa_family;
	char        sa_data[];
};


#define S_IFMT   0170000
#define S_IFIFO  0010000
#define S_IFCHR  0020000
#define S_IFDIR  0040000
#define S_IFBLK  0060000
#define S_IFREG  0100000
#define S_IFLNK  0120000
#define S_IFSOCK 0140000

#define S_BLKSIZE  512

#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#define S_ISCHR(m) (((m) & 0170000) == 0020000)
#define S_ISBLK(m) (((m) & 0170000) == 0060000)
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#define S_ISFIFO(m) (((m) & 0170000) == 0010000)
#define S_ISLNK(m) (((m) & 0170000) == 0120000)
#define S_ISSOCK(m) (((m) & 0170000) == 0140000)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef int dev_t;

typedef int pid_t;
typedef int off_t;
typedef long long off64_t;
typedef int mode_t;
typedef int gid_t;
typedef int uid_t;

typedef int dev_t;
typedef int ino_t; /* FIXME: should be unsigned long long to encode id_t? */
typedef int nlink_t;
typedef int blksize_t;
typedef int blkcnt_t;


struct stat {
	dev_t     st_dev;
	ino_t     st_ino;
	mode_t    st_mode;
	nlink_t   st_nlink;
	uid_t     st_uid;
	gid_t     st_gid;
	dev_t     st_rdev;
	off_t     st_size;
	time_t    st_atime;
	time_t    st_mtime;
	time_t    st_ctime;
	blksize_t st_blksize;
	blkcnt_t  st_blocks;
};


struct timeval {
};
#endif
