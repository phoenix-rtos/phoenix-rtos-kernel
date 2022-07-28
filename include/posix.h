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

enum { F_DUPFD = 0, F_DUPFD_CLOEXEC, F_GETFD, F_SETFD, F_GETFL, F_SETFL,
       F_GETOWN, F_SETOWN, F_GETLK, F_SETLK, F_SETLKW };

#define FD_CLOEXEC 1

#define O_RDONLY   0x0001
#define O_WRONLY   0x0002
#define O_RDWR     0x0004
#define O_APPEND   0x0008
#define O_CREAT    0x0100
#define O_TRUNC    0x0200
#define O_EXCL     0x0400
#define O_SYNC     0x0800
#define O_NONBLOCK 0x1000
#define O_NOCTTY   0x2000
#define O_CLOEXEC  0x4000

typedef int ssize_t;

typedef size_t socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr {
	sa_family_t sa_family;
	char sa_data[14];
};


/* File type */
#define S_IFMT   0xf000 /* File type mask */
#define S_IFSOCK 0xc000 /* Socket */
#define S_IFLNK  0xa000 /* Symbolic link */
#define S_IFREG  0x8000 /* Regular file */
#define S_IFBLK  0x6000 /* Block device */
#define S_IFDIR  0x4000 /* Directory */
#define S_IFCHR  0x2000 /* Character device */
#define S_IFIFO  0x1000 /* FIFO */

#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* Socket */
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)  /* Symbolic link */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)  /* Regular file */
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)  /* Block device */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)  /* Directory */
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)  /* Character device */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)  /* FIFO */

/* File permissions */
#define S_ISUID 0x0800 /* Set user ID on execution */
#define S_ISGID 0x0400 /* Set group ID on execution */
#define S_ISVTX 0x0200 /* Sticky bit */

#define S_IRWXU 0x01c0 /* RWX mask for owner */
#define S_IRUSR 0x0100 /* R for owner */
#define S_IWUSR 0x0080 /* W for owner */
#define S_IXUSR 0x0040 /* X for owner */

#define S_IRWXG 0x0038 /* RWX mask for group */
#define S_IRGRP 0x0020 /* R for group */
#define S_IWGRP 0x0010 /* W for group */
#define S_IXGRP 0x0008 /* X for group */

#define S_IRWXO 0x0007 /* RWX mask for other */
#define S_IROTH 0x0004 /* R for other */
#define S_IWOTH 0x0002 /* W for other */
#define S_IXOTH 0x0001 /* X for other */

/* BSD compatibility macros */
#define S_ISTXT  S_ISVTX
#define S_IREAD  S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC  S_IXUSR

#define S_BLKSIZE 512 /* Block size (stat.st_blocks unit) */

#define ALLPERMS    (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO) /* 07777 */
#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)                               /* 0777 */
#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) /* 0666 */


#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef int dev_t;

typedef int pid_t;
typedef int mode_t;
typedef int gid_t;
typedef int uid_t;

typedef int dev_t;
typedef int ino_t; /* FIXME: should be unsigned long long to encode id_t? */
typedef int nlink_t;
typedef int blksize_t;
typedef long long blkcnt_t;
typedef long long off64_t;
typedef off64_t off_t;


struct timespec {
	time_t tv_sec;
	long tv_nsec;
};


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


#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6  10
#define AF_KEY    15
#define AF_PACKET 17

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6
#define PF_KEY    AF_KEY
#define PF_PACKET AF_PACKET

#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_SEQPACKET 4
#define SOCK_RDM       5

#define SOL_SOCKET 0xFFF

#define SO_RCVBUF 0x1002

#define MSG_PEEK     0x01
#define MSG_WAITALL  0x02
#define MSG_OOB      0x04
#define MSG_DONTWAIT 0x08
#define MSG_MORE     0x10

#define SCM_RIGHTS 1

#define POLLIN     0x1
#define POLLRDNORM 0x2
#define POLLRDBAND 0x4
#define POLLPRI    0x8
#define POLLOUT    0x10
#define POLLWRNORM 0x20
#define POLLWRBAND 0x40
#define POLLERR    0x80
#define POLLHUP    0x100
#define POLLNVAL   0x200


typedef unsigned int nfds_t;


struct pollfd {
	int fd;
	short events;
	short revents;
};


struct iovec {
	void *iov_base;
	size_t iov_len;
};


struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
	int msg_iovlen;
	void *msg_control;
	socklen_t msg_controllen;
	int msg_flags;
};


struct cmsghdr {
	socklen_t cmsg_len;
	int cmsg_level;
	int cmsg_type;
};

#endif
