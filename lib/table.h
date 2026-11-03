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
 * Id to pointer table with lock-free lookups. Add, remove and grow have to be
 * serialized by the caller, which also allocates the blocks, so that it can do
 * it outside its lock (lib_tableNextSize(), lib_tableGrow()).
 *
 * A lookup racing with a grow may miss an id added to the new block, which is
 * fine - nobody could have learned that id yet.
 *
 * NULL marks a free slot, so it cannot be stored.
 */

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


/* NULL if id is free or out of range */
static inline void *lib_tableGet(table_t *t, int id)
{
	table_block_t *block = __atomic_load_n(&t->block, __ATOMIC_ACQUIRE);

	if ((block == NULL) || (id < 0) || ((size_t)id >= block->size)) {
		return NULL;
	}

	return __atomic_load_n(&block->slots[id], __ATOMIC_ACQUIRE);
}


/* Returns the id item was stored under, or -ENOSPC */
int lib_tableAdd(table_t *t, void *item);


void *lib_tableRemove(table_t *t, int id);


/* Size of the block to grow to, or 0 if the table cannot grow */
size_t lib_tableNextSize(table_t *t);


/* -EAGAIN if the table already has size slots, the caller frees block then */
int lib_tableGrow(table_t *t, table_block_t *block, size_t size);


#endif
