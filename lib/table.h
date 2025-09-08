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

#ifndef _PH_LIB_TABLE_H_
#define _PH_LIB_TABLE_H_

#include "hal/types.h"


/*
 * Hands out ids and maps them back to pointers. Lookups are lock free: one
 * acquire load of the current block, a bounds check and one load of the slot.
 * That is the point of using this over a tree, which cannot be walked while it
 * is being rebalanced.
 *
 * Adding, removing and growing are *not* lock free - the caller has to
 * serialize those against each other, and it owns the block memory, which is
 * what lets it allocate outside its lock (lib_tableNextSize(), lib_tableGrow()).
 *
 * A lookup running concurrently with a grow may still hold the older block and
 * therefore miss an id added to the bigger one. That is harmless: an id nobody
 * has learned about yet cannot be looked up on purpose.
 *
 * A NULL entry means "free id", so NULL cannot be stored.
 */

/* Ids are ints, so this is the ceiling on the number of slots. */
#define TABLE_MAX_SIZE ((size_t)1U << ((sizeof(int) * (size_t)__CHAR_BIT__) - 1U))


typedef struct _table_block_t {
	size_t size;
	struct _table_block_t *prev; /* blocks are kept, a lookup may still be reading one */
	void *slots[];
} table_block_t;


typedef struct {
	table_block_t *block;
	size_t first; /* lowest id that may be free - a hint, not a guarantee */
} table_t;


#define TABLE_BLOCK_BYTES(size) (sizeof(table_block_t) + ((size) * sizeof(void *)))


void lib_tableInit(table_t *t);


/* Detaches every block, for the caller to free through prev. */
table_block_t *lib_tableDone(table_t *t);


/* What id holds, or NULL if it is free or out of range. Lock free. */
static inline void *lib_tableGet(table_t *t, int id)
{
	table_block_t *block = __atomic_load_n(&t->block, __ATOMIC_ACQUIRE);

	if ((block == NULL) || (id < 0) || ((size_t)id >= block->size)) {
		return NULL;
	}

	return __atomic_load_n(&block->slots[id], __ATOMIC_ACQUIRE);
}


/* Stores item under a free id and returns it, or -ENOSPC if the table is full. */
int lib_tableAdd(table_t *t, void *item);


/* Frees id and returns what it held. */
void *lib_tableRemove(table_t *t, int id);


/* How many slots to allocate to make room, or 0 if the table cannot grow. */
size_t lib_tableNextSize(table_t *t);


/*
 * Takes block over as the table's storage, copying what the current one holds.
 * Returns 0, or -EAGAIN if the table has already grown past size, in which case
 * the caller frees block itself.
 */
int lib_tableGrow(table_t *t, table_block_t *block, size_t size);


#endif
