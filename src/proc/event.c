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

// sub / output ------- move outside
typedef struct _event_t {
	int fd;
	unsigned type;
	unsigned flags;
	unsigned count;
	unsigned data;
} event_t;


enum { evAdd = 0x1, evDelete = 0x2, evEnable = 0x4, evDisable = 0x8,
	evOneshot = 0x10, evClear = 0x20, evDispatch = 0x40 };


#include HAL
#include "proc.h"
#include "../lib/lib.h"


//#define TRACE(str, ...) lib_printf("event: " str "\n", ##__VA_ARGS__)
#define TRACE(str, ...)


typedef struct _evqueue_t {
	lock_t lock;
	thread_t *threads;
	struct _evnote_t *notes;
	process_t *process;
} evqueue_t;


typedef struct _evnote_t {
	struct _eventry_t *entry;
	struct _evnote_t *next, *prev;
	struct _evqueue_t *queue;
	struct _evnote_t *queue_next, *queue_prev;

	int fd;

	unsigned short mask;
	unsigned short pend;
	unsigned short enabled;
	unsigned short oneshot;
	unsigned short dispatch;

	struct {
		unsigned flags;
		unsigned count;
		unsigned data;
	} pending[16];
} evnote_t;


typedef struct _eventry_t {
	rbnode_t node;
	lock_t lock;
	unsigned refs;
	oid_t oid;

	unsigned short mask;
	evnote_t *notes;
} eventry_t;


static struct {
	lock_t lock;
	rbtree_t notes;
} event_common;


static void common_lock(void)
{
	proc_lockSet(&event_common.lock);
}


static void entry_lock(eventry_t *entry)
{
	proc_lockSet(&entry->lock);
}


static void queue_lock(evqueue_t *queue)
{
	proc_lockSet(&queue->lock);
}


static void common_unlock(void)
{
	proc_lockClear(&event_common.lock);
}


static void entry_unlock(eventry_t *entry)
{
	proc_lockClear(&entry->lock);
}


static void queue_unlock(evqueue_t *queue)
{
	proc_lockClear(&queue->lock);
}


static int event_cmp(rbnode_t *n1, rbnode_t *n2)
{
	eventry_t *e1 = lib_treeof(eventry_t, node, n1);
	eventry_t *e2 = lib_treeof(eventry_t, node, n2);

	if (e1->oid.port != e2->oid.port)
		return (e1->oid.port > e2->oid.port) - (e1->oid.port < e2->oid.port);

	return (e1->oid.id > e2->oid.id) - (e1->oid.id < e2->oid.id);
}


static eventry_t *_entry_find(const oid_t *oid)
{
	eventry_t find, *entry;
	hal_memcpy(&find.oid, oid, sizeof(oid_t));
	if ((entry = lib_treeof(eventry_t, node, lib_rbFind(&event_common.notes, &find.node))) != NULL)
		entry->refs++;
	return entry;
}


static void entry_ref(eventry_t *entry)
{
	common_lock();
	++entry->refs;
	common_unlock();
}


static eventry_t *entry_find(const oid_t *oid)
{
	eventry_t *entry;

	common_lock();
	entry = _entry_find(oid);
	common_unlock();
	return entry;
}


static void _entry_remove(eventry_t *entry)
{
	TRACE("_entry_remove()");

	proc_lockDone(&entry->lock);
	lib_rbRemove(&event_common.notes, &entry->node);
	vm_kfree(entry);
}


static eventry_t *_entry_new(const oid_t *oid)
{
	TRACE("_entry_new()");

	eventry_t *entry;

	if ((entry = vm_kmalloc(sizeof(eventry_t))) == NULL)
		return NULL;

	hal_memset(entry, 0, sizeof(*entry));
	hal_memcpy(&entry->oid, oid, sizeof(oid_t));
	proc_lockInit(&entry->lock);
	entry->refs = 1;
	lib_rbInsert(&event_common.notes, &entry->node);
	return entry;
}


static eventry_t *entry_get(const oid_t *oid)
{
	eventry_t *entry;

	common_lock();
	if ((entry = _entry_find(oid)) == NULL)
		entry = _entry_new(oid);
	common_unlock();
	return entry;
}


static void entry_put(eventry_t *entry)
{
	common_lock();
	if (!--entry->refs)
		_entry_remove(entry);
	common_unlock();
}


static void queue_wakeup(evqueue_t *queue);


static void _entry_register(eventry_t *entry, event_t *event)
{
	TRACE("_entry_register()");

	evnote_t *note;
	unsigned short typebit;

	typebit = 1 << event->type;

	if (!(entry->mask & typebit))
		return;

	note = entry->notes;
	do {
		if (note->mask & typebit) {
			if (note->pend & typebit) {
				note->pending[event->type].flags |= event->flags;
				note->pending[event->type].count += event->count;
			}
			else {
				note->pend |= typebit;
				note->pending[event->type].flags = event->flags;
				note->pending[event->type].count = event->count;

				queue_wakeup(note->queue);
			}

			note->pending[event->type].data = event->data;
		}

		note = note->next;
	} while (note != entry->notes);
}


static int _entry_notify(eventry_t *entry)
{
	TRACE("_entry_notify()");
	int err;
	if ((err = proc_objectSetAttr(&entry->oid, atEvents, &entry->mask, sizeof(entry->mask))) < 0)
		return err;
	return EOK;
}


static int _note_poll(evnote_t *note)
{
	TRACE("_note_poll()");

	unsigned short events;
	int err;

	if ((err = proc_objectGetAttr(&note->entry->oid, atEvents, &events, sizeof(events))) < 0)
		return err;

	/* TODO: have a way to update event data */
	note->pend |= events & note->mask;
	return EOK;
}


static void _entry_recalculate(eventry_t *entry)
{
	TRACE("_entry_recalculate()");

	evnote_t *note;
	unsigned short mask = 0, oldmask;

	if ((note = entry->notes) != NULL) {
		do {
			mask |= note->mask;
			note = note->next;
		} while (note != entry->notes);
	}

	oldmask = entry->mask;
	entry->mask = mask;

	if (mask != oldmask)
		_entry_notify(entry);
}


static evnote_t *_note_new(evqueue_t *queue, int fd, eventry_t *entry)
{
	TRACE("_note_new()");

	evnote_t *note;

	if ((note = vm_kmalloc(sizeof(evnote_t))) == NULL)
		return NULL;

	hal_memset(note, 0, sizeof(*note));
	note->entry = entry;
	note->queue = queue;
	note->fd = fd;

	LIST_ADD(&entry->notes, note);
	LIST_ADD_EX(&queue->notes, note, queue_next, queue_prev);

	return note;
}


static void _note_remove(evnote_t *note)
{
	TRACE("_note_remove()");

	LIST_REMOVE(&note->entry->notes, note);
	entry_put(note->entry);
	LIST_REMOVE_EX(&note->queue->notes, note, queue_next, queue_prev);
	vm_kfree(note);
}


static void _note_merge(evnote_t *note, unsigned flags, unsigned types)
{
	TRACE("_note_merge()");

	if (flags & evAdd) {
		note->mask    |= types;
		note->enabled |= types;
	}

	if (flags & evDelete) {
		note->pend     &= ~types;
		note->mask     &= ~types;
		note->enabled  &= ~types;
		note->oneshot  &= ~types;
		note->dispatch &= ~types;
	}

	if (flags & evEnable)
		note->enabled |= types;

	if (flags & evDisable)
		note->enabled &= ~types;

	if (flags & evOneshot)
		note->oneshot |= types;

	if (flags & evDispatch)
		note->dispatch |= types;

	if (flags & evClear)
		note->pend &= ~types;
}


static int _event_subscribe(evqueue_t *queue, int fd, unsigned flags, unsigned types, const oid_t *oid)
{
	TRACE("_event_subscribe()");

	evnote_t *note;
	eventry_t *entry;
	unsigned short mask;

	if ((note = queue->notes) != NULL) {
		do {
			entry = note->entry;
			if (!hal_memcmp(&entry->oid, oid, sizeof(oid_t))) {
				entry_lock(entry);
				goto got_note;
			}
			note = note->queue_next;
		} while (note != queue->notes);
	}

	/* this reference is donated to the new note created below */
	if ((entry = entry_get(oid)) == NULL)
		return -ENOMEM;

	/* we keep one more reference in case the note gets removed */
	entry_ref(entry);
	entry_lock(entry);

	if ((note = _note_new(queue, fd, entry)) == NULL) {
		entry_unlock(entry);
		entry_put(entry);
		return -ENOMEM;
	}

got_note:
	mask = note->mask;
	_note_merge(note, flags, types);

	if (note->mask != mask) {
		if (mask & ~note->mask) {
			/* change might clear some bits */
			_entry_recalculate(entry);
		}
		else if ((entry->mask & note->mask) != note->mask) {
			entry->mask |= note->mask;
			_entry_notify(entry);
		}
	}

	if (!note->mask)
		_note_remove(note);

	entry_unlock(entry);
	entry_put(entry);

	return EOK;
}


static evqueue_t *queue_create(process_t *process)
{
	TRACE("queue_create()");
	evqueue_t *queue;

	if ((queue = vm_kmalloc(sizeof(evqueue_t))) == NULL)
		return NULL;

	hal_memset(queue, 0, sizeof(*queue));
	queue->process = process;
	proc_lockInit(&queue->lock);
	return queue;
}


static void queue_destroy(evqueue_t *queue)
{
	TRACE("queue_destroy()");

	if (queue->notes != NULL || queue->threads != NULL)
		lib_printf("proc: destroying busy event queue\n");

	proc_lockDone(&queue->lock);
	vm_kfree(queue);
}


static int _event_read(evqueue_t *queue, event_t *event, int eventcnt)
{
	TRACE("_event_read()");

	int type, i = 0;
	unsigned short typebit;
	evnote_t *note;

	if ((note = queue->notes) == NULL)
		return 0;

	do {
		entry_lock(note->entry);
		for (type = 0; type < sizeof(note->pending) / sizeof(*note->pending) && i < eventcnt; ++type) {
			typebit = 1 << type;

			if (note->pend & note->mask & note->enabled & typebit) {
				event->fd = note->fd;
				event->type = type;
				event->flags = note->pending[type].flags;
				event->count = note->pending[type].count;
				event->data = note->pending[type].data;

				if (note->oneshot & typebit) {
					note->mask &= ~typebit;
					_entry_recalculate(note->entry);
				}

				if (note->dispatch & typebit)
					note->enabled &= ~typebit;

				++i;
				++event;
				note->pend &= ~typebit;
			}
		}
		entry_unlock(note->entry);

		note = note->queue_next;
	} while (note != queue->notes && i < eventcnt);

	return i;
}


static void _queue_poll(evqueue_t *queue)
{
	TRACE("_queue_poll()");

	evnote_t *note;

	if ((note = queue->notes) == NULL)
		return;

	do {
		entry_lock(note->entry); /* TODO: is this lock necessary? */
		_note_poll(note);
		entry_unlock(note->entry);

		note = note->queue_next;
	} while (note != queue->notes);
}


static void queue_wakeup(evqueue_t *queue)
{
	proc_threadWakeup(&queue->threads);
}


static void queue_close(evqueue_t *queue)
{
	TRACE("queue_close()");
	eventry_t *entry;

	queue_lock(queue);
	while (queue->notes != NULL) {
		entry_ref(entry = queue->notes->entry);
		entry_lock(entry);
		_note_remove(queue->notes);
		_entry_recalculate(entry);
		entry_unlock(entry);
		entry_put(entry);
	}
	queue_unlock(queue);
	proc_threadBroadcast(&queue->threads);
}


int proc_eventRegister(const oid_t *oid, event_t *event)
{
	eventry_t *entry;

	if ((entry = entry_find(oid)) == NULL)
		return -ENOENT;

	entry_lock(entry);
	_entry_register(entry, event);
	entry_unlock(entry);
	entry_put(entry);
	return EOK;
}


int proc_queueWait(evqueue_t *queue, const event_t *subs, int subcnt, event_t *events, int evcnt, time_t timeout)
{
	int evs;
	oid_t oid;
	process_t *process = proc_current()->process;

	queue_lock(queue);
	for (evs = 0; evs < subcnt; ++evs) {
		if (proc_fileOid(process, subs[evs].fd, &oid) < 0)
			continue; /* TODO: report invalid fildes */

		_event_subscribe(queue, subs[evs].fd, subs[evs].flags, subs[evs].type, &oid);
	}
	_queue_poll(queue);
	while (!(evs = _event_read(queue, events, evcnt))) {
		if ((evs = proc_lockWait(&queue->threads, &queue->lock, timeout)) < 0)
			break;
	}
	queue_unlock(queue);
	return evs;
}


evqueue_t *proc_queueCreate()
{
	return queue_create(proc_current()->process);
}


void event_init(void)
{
	proc_lockInit(&event_common.lock);
	lib_rbInit(&event_common.notes, event_cmp, NULL);
}
