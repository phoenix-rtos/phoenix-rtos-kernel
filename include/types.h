/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * types.h
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_TYPES_H_
#define _PHOENIX_TYPES_H_

typedef unsigned long long id_t;

typedef struct _oid_t {
	unsigned port;
	id_t id;
} oid_t;

typedef int pid_t;
typedef long off_t;
typedef long long off64_t;
typedef int mode_t;
typedef int gid_t;
typedef int uid_t;
typedef int clockid_t;
typedef unsigned int useconds_t;

typedef int dev_t;
typedef int ino_t;
typedef int nlink_t;
typedef int blksize_t;
typedef int blkcnt_t;

#endif
