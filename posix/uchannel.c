/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * UNIX socket data channel
 *
 * Copyright 2026 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "include/errno.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "vm/vm.h"

#include "uchannel.h"


size_t uchannel_roundSize(size_t size)
{
	if ((size != 0U) && ((size & (size - 1U)) != 0U)) {
		size = (size_t)1U << (hal_cpuGetLastBit(size) + 1U);
	}

	return size;
}


uchannel_t *uchannel_alloc(size_t size, int framed)
{
	uchannel_t *ch;
	void *data;

	ch = vm_kmalloc(sizeof(uchannel_t));
	if (ch == NULL) {
		return NULL;
	}

	data = vm_kmalloc(size);
	if (data == NULL) {
		vm_kfree(ch);
		return NULL;
	}

	if (proc_lockInit(&ch->lock, &proc_lockAttrDefault, "unix.channel") < 0) {
		vm_kfree(data);
		vm_kfree(ch);
		return NULL;
	}

	ch->refs = 1;
	ch->framed = (framed != 0) ? 1U : 0U;
	ch->flags = 0;
	ch->fdpacks = NULL;
	ch->rxwait = NULL;
	ch->txwait = NULL;
	_cbuffer_init(&ch->buffer, data, size);

	return ch;
}


uchannel_t *uchannel_ref(uchannel_t *ch)
{
	if (ch != NULL) {
		(void)lib_atomicIncrement(&ch->refs);
	}

	return ch;
}


void uchannel_put(uchannel_t *ch)
{
	fdpack_t *packs;

	if (ch == NULL) {
		return;
	}

	if (lib_atomicDecrement(&ch->refs) != 0) {
		return;
	}

	/*
	 * The last reference is gone, so the channel is unreachable and no lock is
	 * needed. The descriptors are discarded after the channel itself is freed,
	 * as fdpass_discard() reaches back into the file descriptor table.
	 */
	packs = ch->fdpacks;

	(void)proc_lockDone(&ch->lock);
	vm_kfree(ch->buffer.data);
	vm_kfree(ch);

	if (packs != NULL) {
		(void)fdpass_discard(&packs);
	}
}


ssize_t uchannel_write(uchannel_t *ch, const void *buf, size_t len, unsigned int flags, fdpack_t *fdpack)
{
	ssize_t ret = 0;
	int err;

	(void)proc_lockSet(&ch->lock);

	for (;;) {
		if ((ch->flags & (UCHANNEL_SHUT_RD | UCHANNEL_SHUT_WR)) != 0U) {
			ret = -EPIPE;
			break;
		}

		if (len == 0U) {
			ret = 0;
			break;
		}

		if (ch->framed == 0U) {
			ret = (ssize_t)_cbuffer_write(&ch->buffer, buf, len);
		}
		else if (len > (ch->buffer.sz - sizeof(len))) {
			ret = -EMSGSIZE;
			break;
		}
		else if (_cbuffer_free(&ch->buffer) >= (len + sizeof(len))) {
			(void)_cbuffer_write(&ch->buffer, &len, sizeof(len));
			(void)_cbuffer_write(&ch->buffer, buf, len);
			ret = (ssize_t)len;
		}
		else {
			/* not enough room for the whole frame */
		}

		if (ret > 0) {
			if (fdpack != NULL) {
				LIST_ADD(&ch->fdpacks, fdpack);
			}
			(void)proc_threadBroadcast(&ch->rxwait);
			break;
		}

		if ((flags & UCHANNEL_OP_NONBLOCK) != 0U) {
			ret = -EWOULDBLOCK;
			break;
		}

		err = proc_lockWait(&ch->txwait, &ch->lock, 0);
		if (err == -EINTR) {
			/* the lock has not been reacquired */
			return -EINTR;
		}
		if (err < 0) {
			ret = (ssize_t)err;
			break;
		}
	}

	(void)proc_lockClear(&ch->lock);

	return ret;
}


static void uchannel_takePacks(uchannel_t *ch, fdpack_t **packs)
{
	*packs = ch->fdpacks;
	ch->fdpacks = NULL;
}


ssize_t uchannel_read(uchannel_t *ch, void *buf, size_t len, unsigned int flags, fdpack_t **packs)
{
	ssize_t ret = 0;
	size_t rlen = 0;
	int err;

	if (packs != NULL) {
		*packs = NULL;
	}

	(void)proc_lockSet(&ch->lock);

	for (;;) {
		if (len == 0U) {
			/*
			 * Reading nothing must not consume a frame. It still reports
			 * readability instead of waiting for data it would not take.
			 */
			ret = 0;
			if (_cbuffer_avail(&ch->buffer) > 0U) {
				break;
			}
		}
		else if (ch->framed == 0U) {
			if ((flags & UCHANNEL_OP_PEEK) != 0U) {
				ret = (ssize_t)_cbuffer_peek(&ch->buffer, buf, len);
			}
			else {
				ret = (ssize_t)_cbuffer_read(&ch->buffer, buf, len);
			}
		}
		else if (_cbuffer_avail(&ch->buffer) > sizeof(rlen)) {
			(void)_cbuffer_peekAt(&ch->buffer, 0U, &rlen, sizeof(rlen));
			ret = (ssize_t)min(len, rlen);

			if ((flags & UCHANNEL_OP_PEEK) != 0U) {
				(void)_cbuffer_peekAt(&ch->buffer, sizeof(rlen), buf, (size_t)ret);
			}
			else {
				(void)_cbuffer_discard(&ch->buffer, sizeof(rlen));
				(void)_cbuffer_read(&ch->buffer, buf, (size_t)ret);

				if (rlen > (size_t)ret) {
					/* the rest of a truncated frame is dropped */
					(void)_cbuffer_discard(&ch->buffer, rlen - (size_t)ret);
				}
			}
		}
		else {
			/* no complete frame */
		}

		if (ret > 0) {
			if ((flags & UCHANNEL_OP_PEEK) == 0U) {
				if (packs != NULL) {
					uchannel_takePacks(ch, packs);
				}
				(void)proc_threadBroadcast(&ch->txwait);
			}
			break;
		}

		/*
		 * EOS, but only once everything queued has been delivered.
		 * Data written before a shutdown must still be readable.
		 */
		if ((ch->flags & (UCHANNEL_SHUT_WR | UCHANNEL_SHUT_RD)) != 0U) {
			ret = 0;
			break;
		}

		if ((flags & UCHANNEL_OP_NONBLOCK) != 0U) {
			ret = -EWOULDBLOCK;
			break;
		}

		err = proc_lockWait(&ch->rxwait, &ch->lock, 0);
		if (err == -EINTR) {
			/* the lock has not been reacquired */
			return -EINTR;
		}
		if (err < 0) {
			ret = (ssize_t)err;
			break;
		}
	}

	(void)proc_lockClear(&ch->lock);

	return ret;
}


void uchannel_returnPacks(uchannel_t *ch, fdpack_t **packs)
{
	fdpack_t *head, *pack;

	if (*packs == NULL) {
		return;
	}

	(void)proc_lockSet(&ch->lock);

	if (ch->fdpacks == NULL) {
		ch->fdpacks = *packs;
	}
	else {
		/*
		 * The list is circular and `fdpacks` points at its oldest entry, so
		 * appending the leftovers and then moving the head onto the first of
		 * them puts them back in front of anything queued in the meantime.
		 */
		head = *packs;
		do {
			pack = *packs;
			LIST_REMOVE(packs, pack);
			LIST_ADD(&ch->fdpacks, pack);
		} while (*packs != NULL);

		ch->fdpacks = head;
	}

	*packs = NULL;

	(void)proc_lockClear(&ch->lock);
}


/*
 * Discards the descriptor packs queued in the channel. Called once the endpoint
 * that reads the channel is gone, at which point nothing can ever deliver them.
 * No lock is held over fdpass_discard(), which reaches back into the file
 * descriptor table.
 */
void uchannel_discardPacks(uchannel_t *ch)
{
	fdpack_t *packs;

	(void)proc_lockSet(&ch->lock);
	uchannel_takePacks(ch, &packs);
	(void)proc_lockClear(&ch->lock);

	if (packs != NULL) {
		(void)fdpass_discard(&packs);
	}
}


void uchannel_shutWr(uchannel_t *ch)
{
	(void)proc_lockSet(&ch->lock);

	ch->flags |= UCHANNEL_SHUT_WR;

	/* readers see EOS, writers (of a half-closed local end) EPIPE */
	(void)proc_threadBroadcast(&ch->rxwait);
	(void)proc_threadBroadcast(&ch->txwait);

	(void)proc_lockClear(&ch->lock);
}


void uchannel_shutRd(uchannel_t *ch)
{
	(void)proc_lockSet(&ch->lock);

	ch->flags |= UCHANNEL_SHUT_RD;

	(void)proc_threadBroadcast(&ch->txwait);
	(void)proc_threadBroadcast(&ch->rxwait);

	(void)proc_lockClear(&ch->lock);
}


unsigned int uchannel_pollRd(uchannel_t *ch)
{
	unsigned int events = 0;

	(void)proc_lockSet(&ch->lock);

	if ((_cbuffer_avail(&ch->buffer) > 0U) || ((ch->flags & (UCHANNEL_SHUT_WR | UCHANNEL_SHUT_RD)) != 0U)) {
		events |= UCHANNEL_EV_IN;
	}

	if ((ch->flags & UCHANNEL_SHUT_WR) != 0U) {
		events |= UCHANNEL_EV_HUP;
	}

	(void)proc_lockClear(&ch->lock);

	return events;
}


unsigned int uchannel_pollWr(uchannel_t *ch)
{
	unsigned int events = 0;
	size_t free;

	(void)proc_lockSet(&ch->lock);

	if ((ch->flags & (UCHANNEL_SHUT_RD | UCHANNEL_SHUT_WR)) == 0U) {
		free = _cbuffer_free(&ch->buffer);
		if ((ch->framed == 0U) ? (free > 0U) : (free > sizeof(size_t))) {
			events |= UCHANNEL_EV_OUT;
		}
	}

	(void)proc_lockClear(&ch->lock);

	return events;
}


int uchannel_resize(uchannel_t *ch, size_t size)
{
	void *data;
	cbuffer_t old;
	size_t avail, first;

	data = vm_kmalloc(size);
	if (data == NULL) {
		return -ENOMEM;
	}

	(void)proc_lockSet(&ch->lock);

	avail = _cbuffer_avail(&ch->buffer);
	old = ch->buffer;
	_cbuffer_init(&ch->buffer, data, size);

	if (avail <= size) {
		/* copy the buffered bytes over in order */
		if (avail > 0U) {
			first = min(avail, old.sz - old.r);
			(void)_cbuffer_write(&ch->buffer, (const char *)old.data + old.r, first);
			if (avail > first) {
				(void)_cbuffer_write(&ch->buffer, old.data, avail - first);
			}
		}
	}
	else {
		/*
		 * FIXME: the buffered data does not fit the new ring and is dropped
		 * whole, so that a frame is never left half present. The sender's
		 * send() has already reported those bytes as accepted and the reader
		 * has no way to tell they went missing - a byte stream loses a piece
		 * out of its middle, a record socket loses entire records.
		 */
	}

	(void)proc_threadBroadcast(&ch->txwait);

	(void)proc_lockClear(&ch->lock);

	vm_kfree(old.data);

	return 0;
}


size_t uchannel_size(uchannel_t *ch)
{
	size_t size;

	(void)proc_lockSet(&ch->lock);
	size = ch->buffer.sz;
	(void)proc_lockClear(&ch->lock);

	return size;
}
