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
	char full, mark;
	void *data;
	page_t *pages;
} cbuffer_t;


static inline size_t _cbuffer_free(cbuffer_t *buf)
{
	if (buf->w == buf->r)
		return buf->full ? 0 : buf->sz;

	return (buf->r - buf->w + buf->sz) & (buf->sz - 1);
}


static inline size_t _cbuffer_avail(cbuffer_t *buf)
{
	return buf->sz - _cbuffer_free(buf);
}


static inline int _cbuffer_discard(cbuffer_t *buf, size_t sz)
{
	int cnt = min(_cbuffer_free(buf), sz);
	buf->r = (buf->r + cnt) & (buf->sz - 1);
	return cnt;
}


extern int _cbuffer_init(cbuffer_t *buf, void *data, size_t sz);


extern int _cbuffer_read(cbuffer_t *buf, void *data, size_t sz);


extern int _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz);


extern int _cbuffer_peek(const cbuffer_t *buf, void *data, size_t sz);

#endif
