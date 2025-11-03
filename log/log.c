/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Kernel log buffer
 *
 * Copyright 2021 Phoenix Systems
 * Author: Maciej Purski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"

#include "include/ioctl.h"

#include "posix/posix.h"

#include "log.h"
#include "proc/threads.h"
#include "proc/ports.h"

#include <board_config.h>


#ifndef KERNEL_LOG_SIZE
#define KERNEL_LOG_SIZE 2048
#endif

#define TCGETS 0x405c7401UL


typedef struct _log_rmsg_t {
	void *odata;
	oid_t oid;
	msg_rid_t rid;
	size_t osize;
	struct _log_rmsg_t *prev, *next;
} log_rmsg_t;


typedef struct _log_reader_t {
	off_t ridx;
	pid_t pid;
	unsigned int nonblocking;
	log_rmsg_t *msgs;
	struct _log_reader_t *prev, *next;
	int refs;
} log_reader_t;


static struct {
	char buf[KERNEL_LOG_SIZE];
	off_t head;
	off_t tail;
	lock_t lock;
	log_reader_t *readers;
	volatile int updated;
	volatile int enabled;
} log_common;


static int _log_empty(void)
{
	return (log_common.tail == log_common.head) ? 1 : 0;
}


static int _log_full(void)
{
	return ((log_common.tail - log_common.head) == KERNEL_LOG_SIZE) ? 1 : 0;
}


static char _log_pop(void)
{
	return log_common.buf[log_common.head++ % KERNEL_LOG_SIZE];
}


static void _log_push(char c)
{
	log_common.buf[log_common.tail++ % KERNEL_LOG_SIZE] = c;
}


static char _log_getc(off_t off)
{
	return log_common.buf[off % KERNEL_LOG_SIZE];
}


static ssize_t _log_readln(log_reader_t *r, char *buf, size_t sz)
{
	ssize_t n = 0;

	while ((r->ridx < log_common.tail) && ((size_t)n < sz)) {
		buf[n++] = _log_getc(r->ridx++);
		if ((buf[n - 1] == '\n') || (buf[n - 1] == '\0')) {
			break;
		}
	}

	/* Always end with newline */
	if ((n > 0) && (buf[n - 1] != '\n')) {
		if (buf[n - 1] == '\0') {
			buf[n - 1] = '\n';
		}
		else if ((size_t)n < sz) {
			buf[n++] = '\n';
		}
		else {
			buf[n - 1] = '\n';
			r->ridx--;
		}
	}

	return n;
}


static void _log_msgRespond(log_reader_t *r, ssize_t err)
{
	log_rmsg_t *rmsg;
	msg_t msg;

	rmsg = r->msgs;
	LIST_REMOVE(&r->msgs, rmsg);

	msg.i.data = NULL;
	msg.i.size = 0;

	msg.type = mtRead;
	msg.pid = r->pid;
	msg.o.data = rmsg->odata;
	msg.o.size = rmsg->osize;
	msg.o.err = (int)err;

	(void)proc_respond(rmsg->oid.port, &msg, rmsg->rid);

	vm_kfree(rmsg);
}


static log_reader_t *_log_readerFind(pid_t pid)
{
	log_reader_t *r, *ret = NULL;

	r = log_common.readers;
	if (r != NULL) {
		do {
			if (r->pid == pid) {
				ret = r;
				break;
			}
			r = r->next;
		} while (r != log_common.readers);
	}

	if (ret != NULL) {
		++ret->refs;
	}

	return ret;
}


static log_reader_t *log_readerFind(pid_t pid)
{
	log_reader_t *r;

	(void)proc_lockSet(&log_common.lock);
	r = _log_readerFind(pid);
	(void)proc_lockClear(&log_common.lock);

	return r;
}


static void _log_readerPut(log_reader_t **r)
{
	if ((*r) != NULL) {
		--(*r)->refs;
		if ((*r)->refs <= 0) {
			while ((*r)->msgs != NULL) {
				_log_msgRespond((*r), -EIO);
			}
			LIST_REMOVE(&log_common.readers, (*r));
			vm_kfree((*r));
			(*r) = NULL;
		}
	}
}

static void log_readerPut(log_reader_t **r)
{
	(void)proc_lockSet(&log_common.lock);
	_log_readerPut(r);
	(void)proc_lockClear(&log_common.lock);
}


static int log_readerAdd(pid_t pid, unsigned int nonblocking)
{
	log_reader_t *r;

	r = log_readerFind(pid);
	if (r != NULL) {
		log_readerPut(&r);
		return -EINVAL;
	}

	r = vm_kmalloc(sizeof(log_reader_t));
	if (r == NULL) {
		return -ENOMEM;
	}

	hal_memset(r, 0, sizeof(log_reader_t));

	r->nonblocking = nonblocking;
	r->pid = pid;
	r->refs = 1;

	(void)proc_lockSet(&log_common.lock);
	r->ridx = log_common.head;
	LIST_ADD(&log_common.readers, r);
	(void)proc_lockClear(&log_common.lock);

	return 0;
}


static ssize_t _log_read(log_reader_t *r, char *buf, size_t sz)
{
	ssize_t ret;

	/* We need to catch up the ring buffer's head */
	if (r->ridx < log_common.head) {
		ret = -EPIPE;
		r->ridx = log_common.head;
	}
	else {
		ret = _log_readln(r, buf, sz);
	}

	return ret;
}


static ssize_t log_read(log_reader_t *r, char *buf, size_t sz)
{
	ssize_t ret;

	(void)proc_lockSet(&log_common.lock);
	ret = _log_read(r, buf, sz);
	(void)proc_lockClear(&log_common.lock);

	return ret;
}


static void _log_readersUpdate(void)
{
	log_reader_t *r = log_common.readers;
	ssize_t ret;

	if (r != NULL) {
		do {
			while (r->msgs != NULL) {
				ret = _log_read(r, r->msgs->odata, r->msgs->osize);
				if (ret == 0) {
					break;
				}
				_log_msgRespond(r, ret);
			}
			r = r->next;
		} while (r != log_common.readers);
	}
}


static int log_readerBlock(log_reader_t *r, msg_t *msg, oid_t oid, msg_rid_t rid)
{
	log_rmsg_t *rmsg;

	rmsg = vm_kmalloc(sizeof(*rmsg));
	if (rmsg == NULL) {
		return -ENOMEM;
	}

	rmsg->odata = msg->o.data;
	rmsg->osize = msg->o.size;
	rmsg->rid = rid;
	rmsg->oid = oid;

	(void)proc_lockSet(&log_common.lock);
	LIST_ADD(&r->msgs, rmsg);
	(void)proc_lockClear(&log_common.lock);

	return EOK;
}


static void log_close(pid_t pid)
{
	log_reader_t *r;

	(void)proc_lockSet(&log_common.lock);
	r = _log_readerFind(pid);
	if (r != NULL) {
		/* Put 2 times to decrement initial reference too */
		_log_readerPut(&r);
		_log_readerPut(&r);
	}
	(void)proc_lockClear(&log_common.lock);
}


static int log_devctl(msg_t *msg)
{
	/*
	 * We need to handle isatty(), which
	 * only checks if a device responds to TCGETS
	 */
	return (((ioctl_in_t *)msg->i.raw)->request == TCGETS) ? EOK : -EINVAL;
}


void log_msgHandler(msg_t *msg, oid_t oid, msg_rid_t rid)
{
	log_reader_t *r;
	int respond = 1;

	switch (msg->type) {
		case mtOpen:
			if ((msg->i.openclose.flags & O_WRONLY) != 0U) {
				msg->o.err = EOK;
			}
			else {
				msg->o.err = log_readerAdd(msg->pid, msg->i.openclose.flags & O_NONBLOCK);
			}
			break;
		case mtRead:
			r = log_readerFind(msg->pid);
			if (r == NULL) {
				msg->o.err = -EINVAL;
			}
			else {
				msg->o.err = log_read(r, msg->o.data, msg->o.size);
				if (msg->o.err == 0) {
					if (r->nonblocking == 0U) {
						msg->o.err = log_readerBlock(r, msg, oid, rid);
						if (msg->o.err == EOK) {
							respond = 0;
						}
					}
					else {
						msg->o.err = -EAGAIN;
					}
				}

				log_readerPut(&r);
			}
			break;
		case mtWrite:
			msg->o.err = (int)log_write(msg->i.data, msg->i.size);
			log_scrub();
			break;
		case mtClose:
			log_close(msg->pid);
			msg->o.err = 0;
			break;
		case mtDevCtl:
			msg->o.err = log_devctl(msg);
			break;
		default:
			msg->o.err = -EINVAL;
			break;
	}

	if (respond == 1) {
		(void)proc_respond(oid.port, msg, rid);
	}
}


size_t log_write(const char *data, size_t len)
{
	size_t i = 0;
	char c;

	if (log_common.enabled != 0) {
		(void)proc_lockSet(&log_common.lock);

		/* No need to check log_common.enabled again,
		 * it's used only on kernel panic */

		for (i = 0; i < len; ++i) {
			_log_push(data[i]);
			if (_log_full() != 0) {
				do {
					/* Log full, remove oldest line to make space */
					c = _log_pop();
				} while ((c != '\n') && (c != '\0') && (_log_empty() == 0));
			}
		}

		if (i > 0U) {
			log_common.updated = 1;
		}
		(void)proc_lockClear(&log_common.lock);
	}
	else {
		for (i = 0; i < len; ++i) {
			hal_consolePutch(data[i]);
		}
	}

	return len;
}


static void _log_scrub(void)
{
	if (log_common.updated != 0) {
		_log_readersUpdate();
		log_common.updated = 0;
	}
}


void log_scrub(void)
{
	/* Treat log_common.updated as atomic to
	 * avoid taking lock in most cases */
	if (log_common.updated != 0) {
		(void)proc_lockSet(&log_common.lock);
		_log_scrub();
		(void)proc_lockClear(&log_common.lock);
	}
}


void log_scrubTry(void)
{
	/* Treat log_common.updated as atomic to
	 * avoid taking lock in most cases */
	if (log_common.updated != 0) {
		if (proc_lockTry(&log_common.lock) == 0) {
			_log_scrub();
			(void)proc_lockClear(&log_common.lock);
		}
	}
}


void log_disable(void)
{
	log_common.enabled = 0;
}


void _log_init(void)
{
	hal_memset(&log_common, 0, sizeof(log_common));
	(void)proc_lockInit(&log_common.lock, &proc_lockAttrDefault, "log.common");

	log_common.enabled = 1;
}
