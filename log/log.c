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

#include "../hal/hal.h"

#include "../include/ioctl.h"

#include "../posix/posix.h"

#include "log.h"
#include "../proc/threads.h"
#include "../proc/msg.h"
#include "../proc/ports.h"


#ifndef SIZE_LOG
#define SIZE_LOG 2048
#endif

#define TCGETS 0x405c7401

/* Temporary until all uart drivers support reading from kernel log */
#if KLOG_ENABLE

typedef struct _log_rmsg_t {
	void *odata;
	unsigned long rid;
	size_t osize;
	struct _log_rmsg_t *prev, *next;
} log_rmsg_t;


typedef struct _log_reader_t {
	offs_t ridx;
	pid_t pid;
	unsigned nonblocking;
	log_rmsg_t *msgs;
	struct _log_reader_t *prev, *next;
} log_reader_t;


static struct {
	char buf[SIZE_LOG];
	oid_t oid;
	offs_t head;
	offs_t tail;
	lock_t lock;
	log_reader_t *readers;
} log_common;

static int _log_empty(void)
{
	return log_common.tail == log_common.head;
}


static int _log_full(void)
{
	return log_common.tail - log_common.head == SIZE_LOG;
}


static char _log_pop(void)
{
	return log_common.buf[log_common.head++ % SIZE_LOG];
}


static void _log_push(char c)
{
	log_common.buf[log_common.tail % SIZE_LOG] = c;
	log_common.tail++;
	if (_log_full())
		_log_pop();
}


static char _log_getc(offs_t off)
{
	return log_common.buf[off % SIZE_LOG];
}


static ssize_t _log_readln(log_reader_t *r, char *buf, size_t sz)
{
	ssize_t n = 0;

	while (r->ridx < log_common.tail && n < sz) {
		buf[n++] = _log_getc(r->ridx++);
		if (buf[n - 1] == '\n' || buf[n - 1] == '\0')
			break;
	}

	/* Always end with newline */
	if (n > 0 && buf[n - 1] != '\n') {
		if (buf[n - 1] == '\0') {
			buf[n - 1] = '\n';
		}
		else if (n < sz) {
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
	msg.o.io.err = err;

	proc_respond(log_common.oid.port, &msg, rmsg->rid);

	vm_kfree(rmsg);
}


static log_reader_t *log_readerFind(pid_t pid)
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

	return ret;
}


static int log_readerAdd(pid_t pid, unsigned nonblocking)
{
	log_reader_t *r;

	if (log_readerFind(pid) != NULL)
		return -EINVAL;

	r = vm_kmalloc(sizeof(log_reader_t));
	if (r == NULL)
		return -ENOMEM;

	hal_memset(r, 0, sizeof(log_reader_t));

	r->nonblocking = !!nonblocking;
	r->pid = pid;

	proc_lockSet(&log_common.lock);
	r->ridx = log_common.head;
	LIST_ADD(&log_common.readers, r);
	proc_lockClear(&log_common.lock);

	return 0;
}


static void _log_readersUpdate(void)
{
	log_reader_t *r = log_common.readers;
	ssize_t ret;

	if (r != NULL) {
		do {
			if (r->msgs != NULL) {
				ret = _log_readln(r, r->msgs->odata, r->msgs->osize);
				_log_msgRespond(r, ret);
			}
			r = r->next;
		} while (r != log_common.readers);
	}
}


static int log_readerBlock(log_reader_t *r, msg_t *msg, unsigned long rid)
{
	log_rmsg_t *rmsg;

	rmsg = vm_kmalloc(sizeof(*rmsg));
	if (rmsg == NULL)
		return -ENOMEM;

	rmsg->odata = msg->o.data;
	rmsg->osize = msg->o.size;
	rmsg->rid = rid;

	proc_lockSet(&log_common.lock);
	LIST_ADD(&r->msgs, rmsg);
	proc_lockClear(&log_common.lock);

	return EOK;
}


static void log_close(pid_t pid)
{
	log_reader_t *r;

	proc_lockSet(&log_common.lock);
	r = log_readerFind(pid);
	if (r != NULL) {
		while (r->msgs != NULL)
			_log_msgRespond(r, -EIO);
		LIST_REMOVE(&log_common.readers, r);
		vm_kfree(r);
	}
	proc_lockClear(&log_common.lock);
}


static int log_devctl(msg_t *msg)
{
	ioctl_in_t *in;
	ioctl_out_t *out;

	in = (ioctl_in_t *)msg->i.raw;
	out = (ioctl_out_t *)msg->o.raw;
	/*
	 * We need to handle isatty(), which
	 * only checks if a device responds to TCGETS
	 */
	if (in->request == TCGETS)
		out->err = EOK;
	else
		out->err = -EINVAL;

	return 0;
}


static ssize_t log_read(log_reader_t *r, char *buf, size_t sz)
{
	ssize_t ret;

	proc_lockSet(&log_common.lock);
	/* We need to catch up the ring buffer's head */
	if (r->ridx < log_common.head) {
		ret = -EPIPE;
		r->ridx = log_common.head;
	}
	else {
		ret = _log_readln(r, buf, sz);
	}
	proc_lockClear(&log_common.lock);

	return ret;
}


static void log_msgthr(void *arg)
{
	msg_t msg;
	log_reader_t *r;
	unsigned long int rid;
	int respond;

	for (;;) {
		if (proc_recv(log_common.oid.port, &msg, &rid) != 0)
			continue;

		respond = 1;
		switch (msg.type) {
			case mtOpen:
				if (msg.i.openclose.flags & O_WRONLY)
					msg.o.io.err = EOK;
				else
					msg.o.io.err = log_readerAdd(msg.pid, msg.i.openclose.flags & O_NONBLOCK);
				break;
			case mtRead:
				r = log_readerFind(msg.pid);
				if (r == NULL) {
					msg.o.io.err = -EINVAL;
				}
				else {
					msg.o.io.err = log_read(r, msg.o.data, msg.o.size);
					if (msg.o.io.err == 0 && !r->nonblocking) {
						msg.o.io.err = log_readerBlock(r, &msg, rid);
						if (msg.o.io.err == EOK)
							respond = 0;
					}
					else if (msg.o.io.err == 0 && r->nonblocking) {
						msg.o.io.err = -EAGAIN;
					}
				}
				break;
			case mtWrite:
				msg.o.io.err = log_write(msg.i.data, msg.i.size);
				break;
			case mtClose:
				log_close(msg.pid);
				msg.o.io.err = 0;
			case mtDevCtl:
				log_devctl(&msg);
				break;
			default:
				msg.o.io.err = -EINVAL;
				break;
		}
		if (respond)
			proc_respond(log_common.oid.port, &msg, rid);
	}
}

#endif /* KLOG_ENABLE */


int log_write(const char *data, size_t len)
{
	int i = 0;
#if KLOG_ENABLE
	int overwrite = 0;
	char c;

	proc_lockSet(&log_common.lock);

	if (log_common.tail + len >= log_common.head + SIZE_LOG)
		overwrite = 1;

	for (i = 0; i < len; ++i)
		_log_push(data[i]);

	if (overwrite) {
		do {
			c = _log_pop();
		} while (c != '\n' && c != '\0' && !_log_empty());
	}

	if (i > 0)
		_log_readersUpdate();
	proc_lockClear(&log_common.lock);
#else
	for (i = 0; i < len; ++i)
		hal_consolePutch(data[i]);
#endif /* KLOG_ENABLE */

	return len;
}


void _log_start(void)
{
#if KLOG_ENABLE
	/* Create port 0 for /dev/kmsg */
	if (proc_portCreate(&log_common.oid.port) != 0)
		return;

	proc_threadCreate(NULL, log_msgthr, NULL, 4, 2048, NULL, 0, NULL);
#endif /* KLOG_ENABLE */
}


void _log_init(void)
{
#if KLOG_ENABLE
	hal_memset(&log_common, 0, sizeof(log_common));
	proc_lockInit(&log_common.lock);
#endif /* KLOG_ENABLE */
}
