/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System events
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_EVENT_H_
#define _PHOENIX_EVENT_H_

typedef struct _event_t {
	int fd;
	unsigned flags;
	unsigned types;
} event_t;


/* Flags */
enum { evAdd = 0x1, evDelete = 0x2, evOneshot = 0x4, evClear = 0x8};

#endif