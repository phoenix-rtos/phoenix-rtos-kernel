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

#ifndef _LIB_CBUFFER_H_
#define _LIB_CBUFFER_H_

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


static inline size_t _cbuffer_discard(cbuffer_t *buf, size_t sz)
{
	size_t cnt = min(_cbuffer_avail(buf), sz);
	buf->r = (buf->r + cnt) & (buf->sz - 1U);
	if (cnt > 0U) {
		buf->full = 0;
	}
	return cnt;
}


extern unsigned int _cbuffer_init(cbuffer_t *buf, void *data, size_t sz);


extern unsigned int _cbuffer_read(cbuffer_t *buf, void *data, size_t sz);


extern unsigned int _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz);


extern unsigned int _cbuffer_peek(const cbuffer_t *buf, void *data, size_t sz);

#endif
