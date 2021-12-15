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

#include HAL

#include "../include/ioctl.h"

#include "../posix/posix.h"

#include "klog.h"
#include "../proc/threads.h"
#include "../proc/msg.h"
#include "../proc/ports.h"


#ifndef KLOG_BUFSZ
#define KLOG_BUFSZ (2 * SIZE_PAGE)
#endif


#define TCGETS 0x405c7401


typedef struct klog_readMsg {
	void *odata;
	unsigned long rid;
	size_t osize;
	struct klog_readMsg *prev, *next;
} klog_readMsg_t;


typedef struct klog_reader {
	offs_t ridx;
	unsigned pid;
	unsigned nonblocking;
	klog_readMsg_t *msgs;
	struct klog_reader *prev, *next;
} klog_reader_t;


static struct {
	char buf[KLOG_BUFSZ];
	u32 port;
	offs_t head;
	offs_t tail;
	lock_t lock;
	klog_reader_t *readers;
} klog_common;


static int _fifo_empty(void)
{
	return klog_common.tail == klog_common.head;
}


static int _fifo_full(void)
{
	return klog_common.tail - klog_common.head == KLOG_BUFSZ;
}


static char _fifo_pop(void)
{
	return klog_common.buf[klog_common.head++ % KLOG_BUFSZ];
}


static void _fifo_push(char c)
{
	klog_common.buf[klog_common.tail % KLOG_BUFSZ] = c;
	klog_common.tail++;
	if (_fifo_full())
		_fifo_pop();
}


static char _fifo_get(offs_t off)
{
	return klog_common.buf[off % KLOG_BUFSZ];
}


void _klog_init(void)
{
	hal_memset(&klog_common, 0, sizeof(klog_common));
	proc_lockInit(&klog_common.lock);
}


static klog_reader_t *klog_readerFind(unsigned pid)
{
	klog_reader_t *r, *ret = NULL;

	r = klog_common.readers;
	if (r != NULL) {
		do {
			if (r->pid == pid) {
				ret = r;
				break;
			}
			r = r->next;
		} while (r != klog_common.readers);
	}

	return ret;
}


static int klog_readerAdd(unsigned pid, unsigned nonblocking)
{
	klog_reader_t *r;

	if (klog_readerFind(pid) != NULL)
		return -EINVAL;

	r = vm_kmalloc(sizeof(klog_reader_t));
	if (r == NULL)
		return -ENOMEM;

	hal_memset(r, 0, sizeof(klog_reader_t));

	r->nonblocking = !!nonblocking;
	r->pid = pid;

	proc_lockSet(&klog_common.lock);
	r->ridx = klog_common.head;
	LIST_ADD(&klog_common.readers, r);
	proc_lockClear(&klog_common.lock);

	return 0;
}


static ssize_t _klog_readln(klog_reader_t *r, char *buf, size_t sz)
{
	ssize_t n = 0;

	while (r->ridx < klog_common.tail && n < sz) {
		buf[n++] = _fifo_get(r->ridx++);
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


static ssize_t klog_read(klog_reader_t *r, char *buf, size_t sz)
{
	ssize_t ret;

	proc_lockSet(&klog_common.lock);
	/* We need to catch up the ring buffer's head */
	if (r->ridx < klog_common.head) {
		ret = -EPIPE;
		r->ridx = klog_common.head;
	}
	else {
		ret = _klog_readln(r, buf, sz);
	}
	proc_lockClear(&klog_common.lock);

	return ret;
}


static void _klog_msgRespond(klog_reader_t *r, ssize_t err)
{
	klog_readMsg_t *rmsg;
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

	proc_respond(klog_common.port, &msg, rmsg->rid);

	vm_kfree(rmsg);
}


static void _klog_updateReaders(void)
{
	klog_reader_t *r = klog_common.readers;
	ssize_t ret;

	if (r != NULL) {
		do {
			if (r->msgs != NULL) {
				ret = _klog_readln(r, r->msgs->odata, r->msgs->osize);
				_klog_msgRespond(r, ret);
			}
			r = r->next;
		} while (r != klog_common.readers);
	}
}


static void klog_close(unsigned pid)
{
	klog_reader_t *r;

	proc_lockSet(&klog_common.lock);
	r = klog_readerFind(pid);
	if (r != NULL) {
		while (r->msgs != NULL)
			_klog_msgRespond(r, -EIO);
		LIST_REMOVE(&klog_common.readers, r);
		vm_kfree(r);
	}
	proc_lockClear(&klog_common.lock);
}


static int klog_devctl(msg_t *msg)
{
	ioctl_in_t *in;
	ioctl_out_t *out;

	in = (ioctl_in_t *)msg->i.raw;
	out = (ioctl_out_t *)msg->o.raw;
	/*
	 * We need to handle isatty(), which
	 * only checks if a device responds to TCGETS
	 */
	if (in->request == TCGETS) {
		out->err = EOK;
	}
	else {
		out->err = -EINVAL;
	}

	return 0;
}


int klog_write(const char *data, size_t len)
{
#if KLOG_ENABLE
	int i = 0, overwrite = 0;
	char c;

	proc_lockSet(&klog_common.lock);

	if (klog_common.tail + len >= klog_common.head + KLOG_BUFSZ)
		overwrite = 1;

	for (i = 0; i < len; ++i)
		_fifo_push(data[i]);

	if (overwrite) {
		do {
			c = _fifo_pop();
		} while (c != '\n' && c != '\0' && !_fifo_empty());
	}

	if (i > 0)
		_klog_updateReaders();
	proc_lockClear(&klog_common.lock);
#else
	hal_consolePrint(ATTR_NORMAL, data);
#endif /* KLOG_ENABLE */

	return len;
}


static int klog_readerBlock(klog_reader_t *r, msg_t *msg, unsigned long rid)
{
	klog_readMsg_t *rmsg;

	rmsg = vm_kmalloc(sizeof(*rmsg));
	if (rmsg == NULL)
		return -ENOMEM;

	rmsg->odata = msg->o.data;
	rmsg->osize = msg->o.size;
	rmsg->rid = rid;

	proc_lockSet(&klog_common.lock);
	LIST_ADD(&r->msgs, rmsg);
	proc_lockClear(&klog_common.lock);

	return EOK;
}


static void msgthr(void *arg)
{
	msg_t msg;
	klog_reader_t *r;
	unsigned long int rid;
	int respond;

	for (;;) {
		if (proc_recv(klog_common.port, &msg, &rid) != 0)
			continue;

		respond = 1;
		switch (msg.type) {
			case mtOpen:
				if (msg.i.openclose.flags & O_WRONLY)
					msg.o.io.err = EOK;
				else
					msg.o.io.err = klog_readerAdd(msg.pid, msg.i.openclose.flags & O_NONBLOCK);
				break;
			case mtRead:
				r = klog_readerFind(msg.pid);
				if (r == NULL) {
					msg.o.io.err = -EINVAL;
				}
				else {
					msg.o.io.err = klog_read(r, msg.o.data, msg.o.size);
					if (msg.o.io.err == 0 && !r->nonblocking) {
						msg.o.io.err = klog_readerBlock(r, &msg, rid);
						if (msg.o.io.err == EOK)
							respond = 0;
					}
					else if (msg.o.io.err == 0 && r->nonblocking) {
						msg.o.io.err = -EAGAIN;
					}
				}
				break;
			case mtWrite:
				msg.o.io.err = klog_write(msg.i.data, msg.i.size);
				break;
			case mtClose:
				klog_close(msg.pid);
				msg.o.io.err = 0;
			case mtDevCtl:
				klog_devctl(&msg);
				break;
			default:
				msg.o.io.err = -EINVAL;
				break;
		}
		if (respond)
			proc_respond(klog_common.port, &msg, rid);
	}
}


void _klog_initSrv(void)
{
#if KLOG_ENABLE
	/* Create port 0 for /dev/kmsg */
	if (proc_portCreate(&klog_common.port) != 0)
		return;

	proc_threadCreate(NULL, msgthr, NULL, 4, 2048, NULL, 0, NULL);
#endif /* KLOG_ENABLE */
}
