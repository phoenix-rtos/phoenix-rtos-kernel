/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Table of pointers indexed by an allocated id
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/hal.h"
#include "include/errno.h"

#include "assert.h"
#include "table.h"


#define TABLE_MIN_SIZE 8


void lib_tableInit(table_t *t)
{
	t->block = NULL;
	t->first = 0;
}


table_block_t *lib_tableDone(table_t *t)
{
	table_block_t *block = t->block;

	lib_tableInit(t);

	return block;
}


static int _lib_tableClaim(table_block_t *block, size_t from, void *item)
{
	void *empty;
	size_t id;

	for (id = from; id < block->size; id++) {
		empty = NULL;
		if (__atomic_compare_exchange_n(&block->slots[id], &empty, item, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED) != 0) {
			return (int)id;
		}
	}

	return -ENOSPC;
}


int lib_tableAdd(table_t *t, void *item)
{
	table_block_t *block = t->block;
	int id;

	LIB_ASSERT(item != NULL, "a free id is a NULL slot, so NULL cannot be stored");

	if (block == NULL) {
		return -ENOSPC;
	}

	id = _lib_tableClaim(block, t->first, item);
	if ((id < 0) && (t->first != 0)) {
		/* first is only a hint, so make sure the table really is full */
		id = _lib_tableClaim(block, 0, item);
	}

	if (id >= 0) {
		t->first = (size_t)id + 1U;
	}

	return id;
}


void *lib_tableRemove(table_t *t, int id)
{
	table_block_t *block = t->block;
	void *item;

	if ((block == NULL) || (id < 0) || ((size_t)id >= block->size)) {
		return NULL;
	}

	item = __atomic_exchange_n(&block->slots[id], NULL, __ATOMIC_ACQ_REL);

	if ((item != NULL) && ((size_t)id < t->first)) {
		t->first = (size_t)id;
	}

	return item;
}


size_t lib_tableNextSize(table_t *t)
{
	size_t size = (t->block == NULL) ? (size_t)TABLE_MIN_SIZE : (2U * t->block->size);

	return (size > TABLE_MAX_SIZE) ? 0U : size;
}


int lib_tableGrow(table_t *t, table_block_t *block, size_t size)
{
	table_block_t *old = t->block;

	if ((old != NULL) && (size <= old->size)) {
		return -EAGAIN;
	}

	block->size = size;
	block->prev = old;

	if (old != NULL) {
		hal_memcpy(block->slots, old->slots, old->size * sizeof(void *));
		hal_memset(&block->slots[old->size], 0, (size - old->size) * sizeof(void *));
	}
	else {
		hal_memset(block->slots, 0, size * sizeof(void *));
	}

	/* the block is complete before it becomes the one lookups use */
	__atomic_store_n(&t->block, block, __ATOMIC_RELEASE);

	return 0;
}
