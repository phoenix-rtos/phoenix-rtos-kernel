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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_IDTREE_H_
#define _PHOENIX_IDTREE_H_

#include "rb.h"


#define MAX_ID ((1LL << (__CHAR_BIT__ * (sizeof(int)) - 1)) - 1)


typedef rbtree_t idtree_t;


typedef struct {
	rbnode_t linkage;
	int lmaxgap, rmaxgap;
	int id;
} idnode_t;


#define lib_idtreeof(type, node_field, node) ({ \
	long _off = (long)&(((type *)0)->node_field); \
	idnode_t *tmpnode = (node); \
	(type *)((tmpnode == NULL) ? NULL : ((void *)tmpnode - _off)); \
})


idnode_t *lib_idtreeFind(idtree_t *tree, int id);


idnode_t *lib_idtreeMinimum(rbnode_t *node);


idnode_t *lib_idtreeNext(rbnode_t *node);


int lib_idtreeInsert(idtree_t *tree, idnode_t *z);


void lib_idtreeRemove(idtree_t *tree, idnode_t *node);


int lib_idtreeId(idnode_t *node);


int lib_idtreeAlloc(idtree_t *tree, idnode_t *n, int min);


void lib_idtreeInit(idtree_t *tree);

#endif
