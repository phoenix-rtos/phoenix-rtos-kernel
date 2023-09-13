/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX server events
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_EVENTS_H_
#define _PHOENIX_POSIX_EVENTS_H_

#include "types.h"


enum { evtDataOut = 0, evtDataIn, evtError, evtGone };


enum { evAdd = 0x1, evDelete = 0x2, evEnable = 0x4, evDisable = 0x8, evOneshot = 0x10, evClear = 0x20, evDispatch = 0x40 };


typedef struct {
	oid_t oid;
	unsigned int flags;
	unsigned short types;
} evsub_t;


typedef struct {
	oid_t oid;
	unsigned int type;
	unsigned int flags;
	unsigned int count;
	unsigned int data;
} event_t;


typedef struct {
	int eventcnt;
	int subcnt;
	int timeout;
	evsub_t subs[];
} event_ioctl_t;


#endif
