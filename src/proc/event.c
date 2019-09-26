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


/* TODO: move outside */
typedef struct _event_t {
	oid_t oid;
	unsigned type;

	unsigned flags;
	unsigned count;
	unsigned data;
} event_t;

typedef struct {
	oid_t oid;
	unsigned flags;
	unsigned short types;
} evsub_t;



// sub / output
typedef struct _event_t {
	int fd;
	unsigned type;
	unsigned flags;
	unsigned count;
	unsigned data;
} event_t;





//#define TRACE(str, ...) lib_printf("event: " str "\n", ##__VA_ARGS__)
#define TRACE(str, ...)


typedef struct _evqueue_t {
	struct _evqueue_t *next, *prev;

//	object_t object;
	lock_t lock;
	request_t *requests;
	struct _evnote_t *notes;
} evqueue_t;


typedef struct _evnote_t {
	struct _eventry_t *entry;
	struct _evnote_t *next, *prev;
	struct _evqueue_t *queue;
	struct _evnote_t *queue_next, *queue_prev;

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
	oid_t oid;
	lock_t lock;
	unsigned refs;

	unsigned short mask;
	evnote_t *notes;
} eventry_t;

#if 0
static handler_t sink_create_op, sink_write_op, sink_open_op, sink_close_op;
static handler_t queue_read_op, queue_write_op, queue_close_op /*, queue_devctl_op*/;
static handler_t qmx_open_op;


static operations_t sink_ops = {
	.handlers = { NULL },
	.open = sink_open_op,
	.close = sink_close_op,
	.write = sink_write_op,
	.create = sink_create_op,
};


static void queue_timeout_op(request_t *r);
static void queue_destroy(object_t *o);

static operations_t queue_ops = {
	.handlers = { NULL },
	.close = queue_close_op,
	.write = queue_write_op,
	.read = queue_read_op,
	/* .devctl = queue_devctl_op, */
	.timeout = queue_timeout_op,
	.release = queue_destroy,
};


static operations_t qmx_ops = {
	.handlers = { NULL },
	.open = qmx_open_op,
};
#endif

static struct {
	object_t sink;
	object_t qmx;
	handle_t lock;
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


static inline evqueue_t *evqueue(object_t *o)
{
	return (evqueue_t *)((char *)o - offsetof(evqueue_t, object));
}


static int event_cmp(rbnode_t *n1, rbnode_t *n2)
{
	eventry_t *e1 = lib_treeof(eventry_t, node, n1);
	eventry_t *e2 = lib_treeof(eventry_t, node, n2);

	if (e1->oid.port != e2->oid.port)
		return (e1->oid.port > e2->oid.port) - (e1->oid.port < e2->oid.port);

	return (e1->oid.id > e2->oid.id) - (e1->oid.id < e2->oid.id);
}


static void queue_add(evqueue_t *queue, evqueue_t **wakeq)
{
	TRACE("queue_add()");

	lock_common();
	if (queue->next == NULL) {
		object_ref(&queue->object);
		LIST_ADD(wakeq, queue);
	}
	unlock_common();
}


static eventry_t *_entry_find(oid_t *oid)
{
	eventry_t find, *entry;
	hal_memcpy(&find.oid, oid, sizeof(oid_t));
	if ((entry = lib_treeof(eventry_t, node, lib_rbFind(&event_common.notes, &find.node))) != NULL)
		entry->refs++;
	return entry;
}


static void entry_ref(eventry_t *entry)
{
	lock_common();
	++entry->refs;
	unlock_common();
}


static eventry_t *entry_find(oid_t *oid)
{
	eventry_t *entry;

	lock_common();
	entry = _entry_find(oid);
	unlock_common();
	return entry;
}


static void _entry_remove(eventry_t *entry)
{
	TRACE("_entry_remove()");

	proc_lockDone(&entry->lock);
	lib_rbRemove(&event_common.notes, &entry->node);
	vm_kfree(entry);
}


static eventry_t *_entry_new(oid_t *oid)
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


static eventry_t *entry_get(oid_t *oid)
{
	eventry_t *entry;

	lock_common();
	if ((entry = _entry_find(oid)) == NULL)
		entry = _entry_new(oid);
	unlock_common();
	return entry;
}


static void entry_put(eventry_t *entry)
{
	lock_common();
	if (!--entry->refs)
		_entry_remove(entry);
	unlock_common();
}


static void _entry_register(eventry_t *entry, event_t *event, evqueue_t **wakeq)
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

				queue_add(note->queue, wakeq);
			}

			note->pending[event->type].data = event->data;
		}

		note = note->next;
	} while (note != entry->notes);
}


static void _entry_notify(eventry_t *entry)
{
	TRACE("_entry_notify()");

	msg_t msg;

	msg.type = mtSetAttr;
	msg.i.attr.type = atEventMask;
	hal_memcpy(&msg.i.attr.oid, &entry->oid, sizeof(oid_t));
	msg.i.attr.val = entry->mask;

	msg.i.data = msg.o.data = NULL;
	msg.i.size = msg.o.size = 0;

	msgSend(entry->oid.port, &msg);
}


static void _note_poll(evnote_t *note)
{
	TRACE("_note_poll()");

	/* TODO: only poll events known to be level triggered? */
	msg_t msg;

	msg.type = mtGetAttr;
	hal_memcpy(&msg.i.attr.oid, &note->entry->oid, sizeof(oid_t));
	msg.i.attr.type = atPollStatus;

	msg.i.data = msg.o.data = NULL;
	msg.i.size = msg.o.size = 0;

	/* TODO: have a way to update event data */

	if (msgSend(note->entry->oid.port, &msg) == EOK && msg.o.attr.val > 0)
		note->pend |= msg.o.attr.val & note->mask;
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


static evnote_t *_note_new(evqueue_t *queue, eventry_t *entry)
{
	TRACE("_note_new()");

	evnote_t *note;

	if ((note = vm_kmalloc(sizeof(evnote_t))) == NULL)
		return NULL;

	hal_memset(note, 0, sizeof(*note));
	note->entry = entry;
	object_ref(&queue->object);
	note->queue = queue;

	LIST_ADD(&entry->notes, note);
	LIST_ADD_EX(&queue->notes, note, queue_next, queue_prev);

	return note;
}


static void _note_remove(evnote_t *note)
{
	TRACE("_note_remove()");

	LIST_REMOVE(&note->entry->notes, note);
	entry_put(note->entry);
	object_put(&note->queue->object);

	LIST_REMOVE_EX(&note->queue->notes, note, queue_next, queue_prev);
	vm_kfree(note);
}


static void _note_merge(evnote_t *note, evsub_t *sub)
{
	TRACE("_note_merge()");

	if (sub->flags & evAdd) {
		note->mask    |= sub->types;
		note->enabled |= sub->types;
	}

	if (sub->flags & evDelete) {
		note->pend     &= ~sub->types;
		note->mask     &= ~sub->types;
		note->enabled  &= ~sub->types;
		note->oneshot  &= ~sub->types;
		note->dispatch &= ~sub->types;
	}

	if (sub->flags & evEnable)
		note->enabled |= sub->types;

	if (sub->flags & evDisable)
		note->enabled &= ~sub->types;

	if (sub->flags & evOneshot)
		note->oneshot |= sub->types;

	if (sub->flags & evDispatch)
		note->dispatch |= sub->types;

	if (sub->flags & evClear)
		note->pend &= ~sub->types;
}


static int _event_subscribe(evqueue_t *queue, evsub_t *sub, int count)
{
	TRACE("_event_subscribe()");

	evnote_t *note;
	eventry_t *entry;
	unsigned short mask;

	while (count--) {
		if ((note = queue->notes) != NULL) {
			do {
				entry = note->entry;
				if (!hal_memcmp(&entry->oid, &sub->oid, sizeof(oid_t))) {
					entry_lock(entry);
					goto got_note;
				}
				note = note->queue_next;
			} while (note != queue->notes);
		}

		/* this reference is donated to the new note created below */
		if ((entry = entry_get(&sub->oid)) == NULL)
			return -ENOMEM;

		/* we keep one more reference in case the note gets removed */
		entry_ref(entry);
		entry_lock(entry);

		if ((note = _note_new(queue, entry)) == NULL) {
			entry_unlock(entry);
			entry_put(entry);
			return -ENOMEM;
		}

	got_note:
		mask = note->mask;
		_note_merge(note, sub);

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
		sub++;
	}

	return EOK;
}


static void queue_wakeup(evqueue_t *queue);


void event_register(event_t *events, int count)
{
	TRACE("_event_register()");

	event_t *event;
	eventry_t *entry;
	evqueue_t *wakeq = NULL;
	int i = 0;

	for (i = 0; i < count; ++i) {
		event = events + i;

		if ((entry = entry_find(&event->oid)) == NULL)
			continue;

		entry_lock(entry);
		_entry_register(entry, event, &wakeq);
		entry_unlock(entry);

		entry_put(entry);
	}

	queue_wakeup(wakeq);
}


static evqueue_t *queue_create(void)
{
	TRACE("queue_create()");

	evqueue_t *queue;

	if ((queue = vm_kmalloc(sizeof(evqueue_t))) == NULL)
		return NULL;

	hal_memset(queue, 0, sizeof(*queue));

	if (proc_lockInit(&queue->lock) < 0) {
		vm_kfree(queue);
		return NULL;
	}

	object_create(&queue->object, &queue_ops);
	object_put(&queue->object);
	return queue;
}


static void queue_destroy(object_t *o)
{
	TRACE("queue_destroy()");

	evqueue_t *queue = evqueue(o);

	if (queue->notes != NULL || queue->requests != NULL)
		printf("posixsrv/event.c error: destroying busy queue\n");

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
				hal_memcpy(&event->oid, &note->entry->oid, sizeof(oid_t));
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


static int queue_unpack(msg_t *msg, evsub_t **subs, int *subcnt, event_t **events, int *evcnt, int *timeout)
{
	if (msg->type == mtRead || msg->type == mtWrite) {
		if (subs != NULL) {
			*subs = msg->i.data;
			*subcnt = msg->i.size / sizeof(evsub_t);
		}

		if (events != NULL) {
			*events = msg->o.data;
			*evcnt = msg->o.size / sizeof(event_t);
		}

		if (timeout != NULL)
			*timeout = (int)msg->i.io.len; /* FIXME: hack! */
	}
#if 0
	else if (msg->type == mtDevCtl) {
		unsigned request;
		event_ioctl_t *ioctl;

		ioctl = (event_ioctl_t *)ioctl_unpack2(msg, &request, NULL, events);
		/* TODO: check request */

		if (subs != NULL) {
			*subs = ioctl->subs;
			*subcnt = ioctl->subcnt;
		}

		if (events != NULL)
			*evcnt = ioctl->eventcnt;

		if (timeout != NULL)
			*timeout = ioctl->timeout;
	}
#endif
	else return -EINVAL;

	return EOK;
}


static void queue_wakeup(evqueue_t *queue)
{
	TRACE("queue_wakeup()");

	request_t *r, *filled = NULL, *empty;
	int count = 0;
	event_t *events;
	evqueue_t *q;

	while ((q = queue) != NULL) {
		empty = NULL;

		queue_lock(queue);
		while (queue->requests != NULL) {
			r = queue->requests;
			LIST_REMOVE(&queue->requests, r);

			if (queue_unpack(&r->msg, NULL, NULL, &events, &count, NULL) < 0)
				continue;

			if ((count = _event_read(queue, events, count))) {
				LIST_ADD(&filled, r);
				rq_setResponse(r, count);
			}
			else {
				LIST_ADD(&empty, r);
			}
		}
		queue->requests = empty;
		queue_unlock(queue);

		lock_common();
		LIST_REMOVE(&queue, queue);
		unlock_common();

		object_put(&q->object);
	}

	while ((r = filled) != NULL) {
		LIST_REMOVE(&filled, r);
		rq_wakeup(r);
	}
}


static request_t *queue_close_op(object_t *o, request_t *r)
{
	TRACE("queue_close_op()");

	evqueue_t *queue = evqueue(o);
	request_t *p;
	eventry_t *entry;

	queue_lock(queue);
	while ((p = queue->requests) != NULL) {
		LIST_REMOVE(&queue->requests, p);
		rq_setResponse(p, -EBADF);
		rq_wakeup(p);
	}

	while (queue->notes != NULL) {
		entry_ref(entry = queue->notes->entry);
		entry_lock(entry);
		_note_remove(queue->notes);
		_entry_recalculate(entry);
		entry_unlock(entry);
		entry_put(entry);
	}
	queue_unlock(queue);

	object_destroy(o);
	return r;
}


static int _queue_readwrite(evqueue_t *queue, evsub_t *subs, int subcnt, event_t *events, int evcnt)
{
	TRACE("_queue_readwrite()");

	if (subcnt)
		_event_subscribe(queue, subs, subcnt);

	if (evcnt) {
		_queue_poll(queue);
		evcnt = _event_read(queue, events, evcnt);
	}

	return evcnt;
}


static request_t *queue_write_op(object_t *o, request_t *r)
{
	TRACE("queue_write_op()");

	evqueue_t *queue = evqueue(o);
	int count = 0;

	event_t *events;
	evsub_t *subs;
	int evcnt, subcnt, timeout;

	if (queue_unpack(&r->msg, &subs, &subcnt, &events, &evcnt, &timeout) < 0) {
		rq_setResponse(r, -EINVAL);
		return r;
	}

	queue_lock(queue);
	if (!(count = _queue_readwrite(queue, subs, subcnt, events, evcnt)) && evcnt && timeout) {
		if (timeout > 0)
			rq_timeout(r, timeout);

		LIST_ADD(&queue->requests, r);
		r = NULL;
	}
	else {
		rq_setResponse(r, count);
	}
	queue_unlock(queue);
	return r;
}


static request_t *queue_read_op(object_t *o, request_t *r)
{
	return queue_write_op(o, r);
}


#if 0
static request_t *queue_devctl_op(object_t *o, request_t *r)
{
	evqueue_t *queue = evqueue(o);

	event_t *events = NULL;
	evsub_t *subs = NULL;
	int count, evcnt = 0, subcnt = 0, timeout = 0;

	queue_unpack(&r->msg, &subs, &subcnt, &events, &evcnt, &timeout);

	queue_lock(queue);
	if (!(count = _queue_readwrite(queue, subs, subcnt, events, evcnt))) {
		LIST_ADD(&queue->requests, r);
		rq_timeout(r, timeout);
		r = NULL;
	}
	else {
		rq_setResponse(r, count);
	}
	queue_unlock(queue);

	return r;
}
#endif


static void queue_timeout_op(request_t *r)
{
	TRACE("queue_timeout_op()");

	evqueue_t *queue = evqueue(r->object);

	queue_lock(queue);
	LIST_REMOVE(&queue->requests, r);
	queue_unlock(queue);
}


static request_t *sink_open_op(object_t *o, request_t *r)
{
	return r;
}


static request_t *sink_close_op(object_t *o, request_t *r)
{
	return r;
}


static request_t *sink_create_op(object_t *o, request_t *r)
{
	evqueue_t *queue;

	if ((queue = queue_create()) == NULL) {
		r->msg.o.create.err = -ENOMEM;
	}
	else {
		r->msg.o.create.err = EOK;
		r->msg.o.create.oid.port = event_common.port;
		r->msg.o.create.oid.id = object_id(&queue->object);
	}

	return r;
}


static request_t *sink_write_op(object_t *o, request_t *r)
{
	TRACE("sink_write()");

	event_t stackbuf[64];
	event_t *events;
	unsigned eventcnt;

	if (r->msg.i.size % sizeof(event_t)) {
		rq_setResponse(r, -EINVAL);
		return r;
	}

	if (r->msg.i.size <= sizeof(stackbuf)) {
		events = stackbuf;
	}
	else if ((events = vm_kmalloc(r->msg.i.size)) == NULL) {
		rq_setResponse(r, -ENOMEM);
		return r;
	}

	eventcnt = r->msg.i.size / sizeof(event_t);
	hal_memcpy(events, r->msg.i.data, r->msg.i.size);
	rq_setResponse(r, EOK);
	rq_wakeup(r);

	event_register(events, eventcnt);

	if (eventcnt > sizeof(stackbuf) / sizeof(event_t))
		vm_kfree(events);

	return NULL;
}


static request_t *qmx_open_op(object_t *o, request_t *r)
{
	evqueue_t *queue;

	if ((queue = queue_create()) == NULL)
		rq_setResponse(r, -ENOMEM);
	else
		rq_setResponse(r, object_id(&queue->object));

	return r;
}





int proc_queueWait(evqueue_t *queue, evsub_t *subs, int subcnt, event_t *events, int evcnt, time_t timeout)
{

}

evqueue_t *proc_queueCreate()
{
	return queue_create();
}



void event_init(void)
{
	proc_lockInit(&event_common.lock);
	lib_rbInit(&event_common.notes, event_cmp, NULL);
}
