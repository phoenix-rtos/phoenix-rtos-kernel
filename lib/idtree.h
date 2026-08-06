/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * id allocating tree
 *
 * Copyright 2018, 2023 Phoenix Systems
 * Author: Jan Sikorski, Aleksander Kaminski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_IDTREE_H_
#define _PH_IDTREE_H_

#include "hal/types.h"
#include "rb.h"

#define MAX_ID ((1ULL << ((size_t)__CHAR_BIT__ * (sizeof(int)) - 1U)) - 1ULL)


typedef rbtree_t idtree_t;


typedef struct {
	rbnode_t linkage;
	int lmaxgap, rmaxgap;
	int id;
} idnode_t;


idnode_t *lib_idtreeFind(idtree_t *tree, int id);


idnode_t *lib_idtreeMinimum(rbnode_t *node);


idnode_t *lib_idtreeNext(rbnode_t *node);


int lib_idtreeInsert(idtree_t *tree, idnode_t *z);


void lib_idtreeRemove(idtree_t *tree, idnode_t *node);


int lib_idtreeId(idnode_t *node);


int lib_idtreeAlloc(idtree_t *tree, idnode_t *n, int minimum);


void lib_idtreeDump(rbnode_t *node);


void lib_idtreeInit(idtree_t *tree);

#endif
