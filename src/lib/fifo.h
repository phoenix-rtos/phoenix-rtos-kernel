/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - FIFO queue
 *
 * Copyright 2019 Phoenix Systems
 * Author: Marek Bialowas, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIB_FIFO_H_
#define _LIB_FIFO_H_

typedef struct _fifo_t {
	unsigned int head;
	unsigned int tail;
	unsigned int size_mask;
	unsigned char *data;
} fifo_t;


/* NOTE: size must be a power of 2 ! */
static inline void fifo_init(fifo_t *f, unsigned int size)
{
	f->head = 0;
	f->tail = 0;
	f->size_mask = size - 1;
}


static inline int fifo_size(fifo_t *f)
{
	return f->size_mask + 1;
}


static inline void fifo_remove_all(fifo_t *f)
{
	f->head = f->tail = 0;
}


static inline void fifo_remove_all_but_one(fifo_t *f)
{
	if (f->head != f->tail)
		f->head = (f->tail + 1) & f->size_mask;
}


static inline unsigned int fifo_is_full(fifo_t *f)
{
	return ((f->head + 1) & f->size_mask) == f->tail;
}


static inline unsigned int fifo_is_empty(fifo_t *f)
{
	return (f->head == f->tail);
}


static inline unsigned int fifo_count(fifo_t *f)
{
	return (f->head - f->tail) & f->size_mask;
}


static inline unsigned int fifo_freespace(fifo_t *f)
{
	return (f->tail - f->head - 1) & f->size_mask;
}


static inline void fifo_push(fifo_t *f, unsigned char byte)
{
	f->data[f->head] = byte;
	f->head = (f->head + 1) & f->size_mask;
}


static inline unsigned char fifo_pop_back(fifo_t *f)
{
	unsigned char ret = f->data[f->tail];
	f->tail = (f->tail + 1) & f->size_mask;

	return ret;
}


static inline unsigned char fifo_pop_front(fifo_t *f)
{
	unsigned int new_head = (f->head - 1) & f->size_mask;
	unsigned char ret = f->data[new_head];
	f->head = new_head;

	return ret;
}


static inline unsigned char fifo_peek_front(fifo_t *f)
{
	unsigned int new_head = (f->head - 1) & f->size_mask;
	unsigned char ret = f->data[new_head];
	return ret;
}


static inline int fifo_has(fifo_t *f, unsigned char byte)
{
	unsigned int tail = f->tail;

	if (fifo_is_empty(f))
		return 0;

	while (tail != f->head) {
		if (f->data[tail] == byte)
			return 1;

		tail = (tail + 1) & f->size_mask;
	}

	return 0;
}


static inline unsigned fifo_read(fifo_t *f, unsigned char *buffer, unsigned bytes)
{
	unsigned count = 0;

	while (!fifo_is_empty(f) && bytes--)
		buffer[count++] = fifo_pop_back(f);

	return count;
}


static inline unsigned fifo_write(fifo_t *f, const unsigned char *buffer, unsigned bytes)
{
	unsigned count = 0;

	while (!fifo_is_full(f) && bytes--)
		fifo_push(f, buffer[count++]);

	return count;
}

#endif
