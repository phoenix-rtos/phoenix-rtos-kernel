/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Events (multiplexed io)
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include HAL
#include "proc.h"
#include "event.h"
#include "../lib/lib.h"
#include "../../include/poll.h"


#define TRACE(str, ...) //lib_printf("event: " str "\n", ##__VA_ARGS__)


static struct {
	lock_t lock;
} event_common;


void poll_init(poll_head_t *poll)
{
	poll->threads = NULL;
	poll->notes = NULL;
}


void _poll_add(poll_head_t *poll, wait_note_t **queue, wait_note_t *note)
{
	note->head = poll;
	note->queue = queue;
	note->events = 0;
	LIST_ADD_EX(&poll->notes, note, thr_next, thr_prev);
	LIST_ADD_EX(queue, note, obj_next, obj_prev);
}


void _poll_remove(wait_note_t *note)
{
	if (note->head != NULL) {
		LIST_REMOVE_EX(note->queue, note, obj_next, obj_prev);
		LIST_REMOVE_EX(&note->head->notes, note, thr_next, thr_prev);
	}
}


void poll_signal(wait_note_t **queue, int events)
{
	wait_note_t *note;

	proc_lockSet(&event_common.lock);
	if ((note = *queue) != NULL) {
		do {
			note->events |= events;
			proc_threadWakeup(note->head->threads);
		}
		while ((note = note->obj_next) != *queue);
	}
	proc_lockClear(&event_common.lock);
}


void poll_lock(void)
{
	proc_lockSet(&event_common.lock);
}


void poll_unlock(void)
{
	proc_lockClear(&event_common.lock);
}


int _poll_wait(poll_head_t *poll, int timeout)
{
	return proc_lockWait(&poll->threads, &event_common.lock, timeout);
}


void _event_init(void)
{
	proc_lockInit(&event_common.lock);
}
