/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spawn
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SPAWN_H_
#define _PHOENIX_SPAWN_H_

#include "posix-types.h"


#define POSIX_SPAWN_SETSCHEDPARAM (1 << 0)
#define POSIX_SPAWN_SETSCHEDULER  (1 << 1)
#define POSIX_SPAWN_SETSIGDEF     (1 << 2)
#define POSIX_SPAWN_SETSIGMASK    (1 << 3)
#define POSIX_SPAWN_SETSID        (1 << 4)
#define POSIX_SPAWN_SETPGROUP     (1 << 5)
#define POSIX_SPAWN_RESETIDS      (1 << 6)  // TODO: delete for now?


typedef struct _sys_spawn_file_action {
	enum { POSIX_SPAWN_OPEN,
		POSIX_SPAWN_CLOSE,
		POSIX_SPAWN_DUP2 } action;
	int fd;
	int dupFd;
	const char *path;
	int oflag;
	mode_t mode;
} sys_spawn_file_action_t;


typedef struct _sys_spawn_attr {
	int flags;
	int fileActionCount;
	sys_spawn_file_action_t *fileActions;
	int sigmask;
	int sigdfl;
	int pgid;
} sys_spawn_attr_t;


#endif
