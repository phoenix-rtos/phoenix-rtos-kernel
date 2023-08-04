/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Unnamed pipes
 *
 * Copyright 2022 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _POSIX_PIPE_H_
#define _POSIX_PIPE_H_

#include "../proc/msg.h"


extern void pipe_msgHandler(msg_t *msg, oid_t oid, unsigned long int rid);


extern void _pipe_init(void);


#endif
