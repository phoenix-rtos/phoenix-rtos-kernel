/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Processes management
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_EVENT_H_
#define _PROC_EVENT_H_

#include "../../include/event.h"
#include "../../include/poll.h"


typedef struct _evqueue_t evqueue_t;


extern evqueue_t *queue_create(struct _process_t *process);


extern int queue_wait(evqueue_t *queue, const struct _event_t *subs, int subcnt, struct _event_t *events, int evcnt, time_t timeout);


extern int proc_eventRegister(const oid_t *oid, unsigned types);


extern int proc_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms);


extern void _event_init(void);

#endif