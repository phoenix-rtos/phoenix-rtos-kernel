/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Circular buffer
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_LIB_CBUFFER_H_
#define _PH_LIB_CBUFFER_H_

#include "attrs.h"

typedef struct {
	size_t sz, r, w;
	unsigned char full, mark;
	void *data;
} cbuffer_t;


static inline size_t _cbuffer_free(cbuffer_t *buf)
{
	if (buf->w == buf->r) {
		return (buf->full != 0U) ? 0U : buf->sz;
	}
	return (buf->r - buf->w + buf->sz) & (buf->sz - 1U);
}


static inline size_t _cbuffer_avail(cbuffer_t *buf)
{
	return buf->sz - _cbuffer_free(buf);
}


/* parasoft-suppress-next-line MISRAC2012-RULE_2_1-h "False positive, function already marked as unused" */
MAYBE_UNUSED static inline size_t _cbuffer_discard(cbuffer_t *buf, size_t sz)
{
	size_t cnt = min(_cbuffer_avail(buf), sz);

	/* FIXME: disallow zero-sized buffers. posix/unix.c uses these but it's a slippery slope */
	LIB_ASSERT(buf->sz > 0U, "attempted to write to zero-sized buffer");

	if (sz == 0U || buf->sz == 0U) {
		return 0U;
	}

	buf->r = (buf->r + cnt) & (buf->sz - 1U);
	if (cnt > 0U) {
		buf->full = 0;
	}
	return cnt;
}


void _cbuffer_init(cbuffer_t *buf, void *data, size_t sz);


size_t _cbuffer_read(cbuffer_t *buf, void *data, size_t sz);


size_t _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz);


size_t _cbuffer_peek(const cbuffer_t *buf, void *data, size_t sz);


#endif
