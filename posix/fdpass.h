/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * File descriptor passing
 *
 * Copyright 2021 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_POSIX_FDPASS_H_
#define _PH_POSIX_FDPASS_H_

#include "posix_private.h"


#define MAX_MSG_CONTROLLEN 256U


/* NOTE: file descriptors are added & removed FIFO style */
typedef struct fdpack_s {
	struct fdpack_s *next, *prev;
	unsigned int first, cnt;
	fildes_t fd[];
} fdpack_t;


int fdpass_pack(fdpack_t **packs, const void *control, socklen_t controllen);


int fdpass_unpack(fdpack_t **packs, void *control, socklen_t *controllen);


int fdpass_discard(fdpack_t **packs);

#endif
