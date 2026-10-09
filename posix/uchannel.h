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

#ifndef _PH_POSIX_UCHANNEL_H_
#define _PH_POSIX_UCHANNEL_H_

#include "hal/hal.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "fdpass.h"
#include "uaddr.h"


/*
 * A uchannel_t is one unidirectional data channel: a ring buffer, the file
 * descriptors travelling behind it and the two wait queues of its endpoints.
 * A connected socket pair owns two of them, one per direction.
 *
 * The channel is the only object shared between the two endpoints, so all
 * cross-endpoint state lives here: the reader learns that no more data will
 * arrive from UCHANNEL_SHUT_WR and the writer learns that nobody will ever read
 * again from UCHANNEL_SHUT_RD - both tested under the very same lock that guards
 * the ring, which is what makes end-of-stream and EPIPE race-free.
 *
 * Everything below `lock` is protected by it. `refs` is atomic and counts the
 * attached endpoints (normally two) plus any transient user. The channel
 * outlives both endpoints if need be, so neither endpoint ever has to look at
 * the other.
 */

/* Channel state flags */
#define UCHANNEL_SHUT_WR (1U << 0) /* no more data will ever be written */
#define UCHANNEL_SHUT_RD (1U << 1) /* no more data will ever be read */

/* Operation flags of uchannel_read()/uchannel_write() */
#define UCHANNEL_OP_NONBLOCK (1U << 0)
#define UCHANNEL_OP_PEEK     (1U << 1)
#define UCHANNEL_OP_TRUNC    (1U << 2)

/*
 * Events reported by uchannel_pollRd()/uchannel_pollWr(). The channel states
 * facts about one direction - deciding what they mean for POLLHUP and POLLOUT
 * is the endpoint's job, as that takes both directions and its own shutdown
 * flags as well.
 */
#define UCHANNEL_EV_IN   (1U << 0) /* a read would not block */
#define UCHANNEL_EV_OUT  (1U << 1) /* there is room, so a write would not block */
#define UCHANNEL_EV_SHUT (1U << 2) /* this direction is shut down, by either of its ends */


typedef struct _uchannel_t {
	int refs; /* atomic */

	u8 framed; /* immutable: 0 - byte stream, 1 - frame */

	u8 sourced; /* immutable: 1 - every frame carries the name of whoever wrote it */

	lock_t lock;

	/* everything below is protected by `lock` */
	u8 flags;
	cbuffer_t buffer;
	fdpack_t *fdpacks;
	thread_t *rxwait;
	thread_t *txwait;
} uchannel_t;


/* Rounds `size` up to a power of two, as required by cbuffer. */
size_t uchannel_roundSize(size_t size);


/* Allocates a channel with one reference. `size` must be a power of two. */
uchannel_t *uchannel_alloc(size_t size, int framed, int sourced);


uchannel_t *uchannel_ref(uchannel_t *ch);


/*
 * Drops one reference. Destroys the channel (and discards any descriptors
 * still queued on it) when the last one is gone. NULL is allowed.
 */
void uchannel_put(uchannel_t *ch);


/*
 * Writes to the channel, blocking until there is room unless
 * UCHANNEL_OP_NONBLOCK is set. Returns the number of bytes written, or -EPIPE,
 * -EMSGSIZE, -EWOULDBLOCK, -EINTR.
 *
 * A frame is written whole or not at all. A byte stream is written whole as
 * well when the call is allowed to block, waiting for the reader to make room
 * as many times as it takes. It reports a short count only when it cannot go
 * on - the reader is gone, the wait was interrupted, or UCHANNEL_OP_NONBLOCK is
 * set - and some bytes have already been handed over.
 *
 * A zero-length write is not turned into a zero-length frame: it only
 * reports -EPIPE or 0.
 *
 * `src` is the name to report as the frame's source. The channel takes the
 * caller's reference over when it writes the frame, and the caller still owns
 * it otherwise. Only a channel allocated with `sourced` can carry a name: one
 * handed to any other channel is neither stored nor given back, so the caller
 * must not pass one.
 *
 * `fdpack`, when given, is queued behind the data and its ownership passes to
 * the channel, but only if the call returns a positive count.
 */
ssize_t uchannel_write(uchannel_t *ch, const void *buf, size_t len, unsigned int flags, uaddr_t *src, fdpack_t *fdpack);


/*
 * Reads from the channel, blocking until data is available unless
 * UCHANNEL_OP_NONBLOCK is set. Returns the number of bytes read, 0 on
 * end-of-stream (the ring is empty and either side is shut down), or
 * -EWOULDBLOCK or -EINTR.
 *
 * Datagram reads consume the entire frame, even if the buffer is too small
 * or has zero length. Any data that does not fit is discarded. If `oflags`
 * is not NULL, MSG_TRUNC is set when the frame is truncated. If
 * UCHANNEL_OP_TRUNC is set, the actual frame length is returned instead of
 * the number of bytes copied.
 *
 * Stream reads consume as many bytes as requested, up to the available
 * data. A zero-length stream read waits for data without consuming it.
 *
 * If `src` is not NULL, the address of the frame sender is returned through
 * it, or NULL if no address is available. The caller owns the returned
 * reference and must release it with uaddr_put(). A peek returns its own
 * reference while leaving the frame in the ring.
 *
 * When `packs` is not NULL the queued descriptor packs are detached into it
 * (the caller unpacks them with no lock held).
 */
ssize_t uchannel_read(uchannel_t *ch, void *buf, size_t len, unsigned int flags, unsigned int *oflags, uaddr_t **src, fdpack_t **packs);


/* Discards the descriptor packs queued in the channel. */
void uchannel_discardPacks(uchannel_t *ch);


/*
 * Declares that nothing will ever be written to the channel again,
 * and wakes everyone waiting on it.
 */
void uchannel_shutWr(uchannel_t *ch);


/*
 * Declares that nothing will ever be read from the channel again,
 * and wakes everyone waiting on it.
 */
void uchannel_shutRd(uchannel_t *ch);


unsigned int uchannel_pollRd(uchannel_t *ch);


unsigned int uchannel_pollWr(uchannel_t *ch);


/*
 * Resizes the ring. Buffered data is kept when it fits the new size and dropped
 * whole when it does not. `size` must be a power of two. Returns -ENOMEM if the
 * new ring cannot be allocated, leaving the channel untouched.
 */
int uchannel_resize(uchannel_t *ch, size_t size);


size_t uchannel_size(uchannel_t *ch);


#endif
