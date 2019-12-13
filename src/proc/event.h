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


typedef struct _poll_head_t {
	struct _thread_t *threads;
	struct _wait_note_t *notes;
} poll_head_t;


typedef struct _wait_note_t {
	struct _wait_note_t **queue;
	struct _wait_note_t *obj_next, *obj_prev;

	struct _poll_head_t *head;
	struct _wait_note_t *thr_next, *thr_prev;

	int events;
} wait_note_t;


extern void poll_init(poll_head_t *poll);


extern void poll_lock(void);


extern void poll_unlock(void);


extern void _poll_add(poll_head_t *poll, wait_note_t **queue, wait_note_t *note);


static inline void poll_add(poll_head_t *poll, wait_note_t **queue, wait_note_t *note)
{
	poll_lock();
	_poll_add(poll, queue, note);
	poll_unlock();
}


extern void _poll_remove(wait_note_t *note);


static inline void poll_remove(wait_note_t *note)
{
	poll_lock();
	_poll_remove(note);
	poll_unlock();
}


extern void poll_signal(wait_note_t **queue, int events);


extern int _poll_wait(poll_head_t *poll, int timeout);


static inline int poll_wait(poll_head_t *poll, int timeout)
{
	int retval;

	poll_lock();
	retval = _poll_wait(poll, timeout);
	poll_unlock();
	return retval;
}


extern void _event_init(void);

#endif