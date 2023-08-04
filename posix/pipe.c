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

#include "pipe.h"

#include "../usrv.h"
#include "../lib/cbuffer.h"
#include "../include/posix.h"


#define SIZE_PIPE_BUFF (2 * SIZE_PAGE)

typedef struct _req_t {
	unsigned long rid;
	msg_t msg;
	struct _req_t *prev, *next;
} req_t;


typedef struct {
	rbnode_t linkage;
	oid_t oid;

	int rrefs;
	int wrefs;

	req_t *wqueue;
	req_t *rqueue;

	lock_t lock;
	void *data;
	cbuffer_t cbuff;
} pipe_t;


static struct {
	rbtree_t pipes;
	unsigned int cnt;
	lock_t lock;
} pipe_common;


static int pipe_cmp(rbnode_t *n1, rbnode_t *n2)
{
	pipe_t *p1 = lib_treeof(pipe_t, linkage, n1);
	pipe_t *p2 = lib_treeof(pipe_t, linkage, n2);
	int res;

	if (p1->oid.id < p2->oid.id) {
		res = -1;
	}
	else if (p1->oid.id > p2->oid.id) {
		res = 1;
	}
	else {
		res = 0;
	}

	return res;
}


static inline pipe_t *pipe_getPipe(const oid_t *oid)
{
	pipe_t p;
	p.oid = *oid;

	return lib_treeof(pipe_t, linkage, lib_rbFind(&pipe_common.pipes, &p.linkage));
}


static inline int pipe_lock(pipe_t *p, unsigned block)
{
	int err = EOK;

	if (block != 0) {
		err = proc_lockTry(&p->lock);
	}
	else {
		err = proc_lockSet(&p->lock);
	}

	return err;
}


static int pipe_wakeup(pipe_t *p, req_t *req, int retVal)
{
	if (req->msg.type == mtRead) {
		LIST_REMOVE(&p->rqueue, req);
	}
	else if (req->msg.type == mtWrite) {
		LIST_REMOVE(&p->wqueue, req);
	}
	else {
		return -EINVAL;
	}

	req->msg.o.io.err = retVal;
	proc_respond(p->oid.port, &req->msg, req->rid);
	vm_kfree(req);

	return EOK;
}


static int pipe_destroy(oid_t oid)
{
	pipe_t *pipe = pipe_getPipe(&oid);
	if (pipe == NULL) {
		return -EINVAL;
	}

	proc_lockSet(&pipe_common.lock);
	lib_rbRemove(&pipe_common.pipes, &pipe->linkage);
	proc_lockClear(&pipe_common.lock);

	proc_lockSet(&pipe->lock);
	_cbuffer_free(&pipe->cbuff);
	vm_kfree(pipe->data);
	proc_lockClear(&pipe->lock);

	proc_lockDone(&pipe->lock);
	vm_kfree(pipe);

	return EOK;
}


static int pipe_create(oid_t *oid)
{
	int res;
	pipe_t *p;

	p = vm_kmalloc(sizeof(pipe_t));
	if (p == NULL) {
		return -ENOMEM;
	}

	p->data = vm_kmalloc(SIZE_PIPE_BUFF);
	if (p->data == NULL) {
		vm_kfree(p);
		return -ENOMEM;
	}

	res = proc_lockInit(&p->lock);
	if (res < 0) {
		vm_kfree(p->data);
		vm_kfree(p);
		return res;
	}

	res = _cbuffer_init(&p->cbuff, p->data, SIZE_PIPE_BUFF);
	if (res < 0) {
		proc_lockClear(&p->lock);
		vm_kfree(p->data);
		vm_kfree(p);
		return res;
	}

	p->oid.port = USRV_PORT;
	p->oid.id = (id_t)(++pipe_common.cnt << USRV_ID_BITS) | USRV_ID_PIPES;

	p->rrefs = 1;
	p->wrefs = 1;

	p->wqueue = NULL;
	p->rqueue = NULL;

	hal_memcpy(oid, &p->oid, sizeof(oid_t));

	proc_lockSet(&pipe_common.lock);
	lib_rbInsert(&pipe_common.pipes, &p->linkage);
	proc_lockClear(&pipe_common.lock);

	return EOK;
}


static int pipe_read(msg_t *msg, unsigned long int rid, int *respond)
{
	int res;
	req_t *req;
	int cbuffFull = 0, bytes, tempSz;
	u8 *buff = msg->o.data;
	size_t sz = msg->o.size;
	unsigned mode = msg->i.io.mode;
	pipe_t *pipe = pipe_getPipe(&msg->i.io.oid);

	if (pipe == NULL || (buff == NULL && sz != 0)) {
		return -EINVAL;
	}

	if (sz == 0) {
		return sz;
	}

	if (pipe_lock(pipe, mode & O_NONBLOCK) < 0) {
		return -EWOULDBLOCK;
	}

	cbuffFull = !_cbuffer_free(&pipe->cbuff);
	bytes = _cbuffer_read(&pipe->cbuff, buff, sz);

	if (bytes < sz) {
		/* Read data from pending writers */
		while (pipe->wqueue != NULL && bytes < sz) {
			tempSz = min(sz - bytes, pipe->wqueue->msg.i.size);
			hal_memcpy(buff + bytes, pipe->wqueue->msg.i.data, tempSz);

			pipe_wakeup(pipe, pipe->wqueue, tempSz);
			bytes += tempSz;
		}
	}

	/* Buffer was full, update writers */
	if (cbuffFull == 1) {
		/* Discharge remaining pending writers */
		while (pipe->wqueue != NULL && _cbuffer_avail(&pipe->cbuff) != 0) {
			tempSz = _cbuffer_write(&pipe->cbuff, pipe->wqueue->msg.i.data, pipe->wqueue->msg.i.size);
			pipe_wakeup(pipe, pipe->wqueue, tempSz);
		}
	}


	if (bytes == 0 && pipe->wrefs == 0) {
		res = -EPIPE;
	}
	else if (bytes == 0 && (mode & O_NONBLOCK)) {
		res = -EWOULDBLOCK;
	}
	/* Add to waiting reading queue */
	else if (bytes == 0) {
		req = vm_kmalloc(sizeof(req_t));
		if (req == NULL) {
			res = -ENOMEM;
		}
		else {
			req->rid = rid;
			hal_memcpy(&req->msg, msg, sizeof(*msg));
			LIST_ADD(&pipe->rqueue, req);
			res = 0;
			*respond = 0;
		}
	}
	else {
		res = bytes;
	}

	proc_lockClear(&pipe->lock);

	return res;
}


static int pipe_write(msg_t *msg, unsigned long int rid, int *respond)
{
	req_t *req;
	int res, tempSz, bytes = 0;
	u8 *buff = msg->i.data;
	size_t sz = msg->i.size;
	unsigned mode = msg->i.io.mode;
	pipe_t *pipe = pipe_getPipe(&msg->i.io.oid);

	if (pipe == NULL || (buff == NULL && sz != 0)) {
		return -EINVAL;
	}

	if (sz == 0) {
		return sz;
	}

	if (pipe_lock(pipe, mode & O_NONBLOCK) < 0) {
		return -EWOULDBLOCK;
	}

	if (pipe->rrefs != 0) {
		/* Write data to pending readers */
		while (pipe->rqueue != NULL && bytes < sz) {
			tempSz = min(sz - bytes, pipe->rqueue->msg.o.size);
			hal_memcpy(pipe->rqueue->msg.o.data, buff + bytes, tempSz);

			pipe_wakeup(pipe, pipe->rqueue, tempSz);
			bytes += tempSz;
		}

		/* Write remaining data to circular buffer */
		bytes += _cbuffer_write(&pipe->cbuff, buff + bytes, sz - bytes);
		if (bytes == 0 && (mode & O_NONBLOCK)) {
			res = -EWOULDBLOCK;
		}
		else if (bytes == 0) {
			req = vm_kmalloc(sizeof(req_t));
			if (req == NULL) {
				res = -ENOMEM;
			}
			else {
				req->rid = rid;
				hal_memcpy(&req->msg, msg, sizeof(*msg));
				LIST_ADD(&pipe->wqueue, req);
				res = 0;
				*respond = 0;
			}
		}
		else {
			res = bytes;
		}
	}
	/* Pipe is broken */
	else {
		res = -EPIPE;
	}

	proc_lockClear(&pipe->lock);

	return res;
}


static int pipe_close(const oid_t *oid, unsigned flags)
{
	/* TODO: handle refs count and destroy pipe */
	return EOK;
}


void pipe_msgHandler(msg_t *msg, oid_t oid, unsigned long int rid)
{
	int response = 1;

	switch (msg->type) {
		case mtOpen:
			/* TODO: handle refs count */
			msg->o.io.err = -ENOSYS;
			break;

		case mtCreate:
			msg->o.create.err = pipe_create(&msg->o.create.oid);
			break;

		case mtRead:
			msg->o.io.err = pipe_read(msg, rid, &response);
			break;

		case mtWrite:
			msg->o.io.err = pipe_write(msg, rid, &response);
			break;

		case mtClose:
			msg->o.io.err = pipe_close(&msg->i.openclose.oid, msg->i.openclose.flags);
			break;

		case mtDevCtl:
		default:
			msg->o.io.err = -ENOSYS;
			break;
	}

	if (response == 1) {
		proc_respond(oid.port, msg, rid);
	}
}


void _pipe_init(void)
{
	pipe_common.cnt = 0;

	proc_lockInit(&pipe_common.lock);
	lib_rbInit(&pipe_common.pipes, pipe_cmp, NULL);
}
