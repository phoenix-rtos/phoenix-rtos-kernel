/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Min/max binary heap
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_BHEAP_H_
#define _PHOENIX_BHEAP_H_


typedef struct _bhnode_t {
	struct _bhnode_t *parent;
	struct _bhnode_t *left;
	struct _bhnode_t *right;
} bhnode_t;


#define lib_bhof(type, nodeField, node) ({ \
	long _off = (long)&(((type *)0)->nodeField); \
	bhnode_t *tmpnode = (node); \
	(type *)((tmpnode == NULL) ? NULL : ((char *)tmpnode - _off)); \
})


typedef int (*bhcomp_t)(bhnode_t *n1, bhnode_t *n2);


typedef void (*bhdump_t)(bhnode_t *node);


typedef struct {
	bhnode_t *root;
	bhnode_t *tail;
	bhcomp_t comp;
} bheap_t;


void lib_bhInsert(bheap_t *heap, bhnode_t *node);


void lib_bhRemove(bheap_t *heap, bhnode_t *node);


bhnode_t *lib_bhPop(bheap_t *heap);


bhnode_t *lib_bhPeek(bheap_t *heap);


void lib_bhDump(bhnode_t *node, bhdump_t dump);


void lib_bhInit(bheap_t *heap, bhcomp_t compare);


#endif
