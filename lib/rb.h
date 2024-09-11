/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - red-black tree
 *
 * Copyright 2017 Phoenix Systems
 * Author: Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIB_RB_H_
#define _LIB_RB_H_


#define lib_treeof(type, node_field, node) ({ \
	long _off = (long)&(((type *)0)->node_field); \
	rbnode_t *tmpnode = (node); \
	(type *)((tmpnode == NULL) ? NULL : ((char *)tmpnode - _off)); \
})


typedef enum _rbcolor_t {
	RB_RED, RB_BLACK
} rbcolor_t;


typedef struct _rbnode_t {
	struct _rbnode_t *parent;
	struct _rbnode_t *left;
	struct _rbnode_t *right;
	rbcolor_t color;
} rbnode_t;


typedef int (*rbcomp_t)(rbnode_t *n1, rbnode_t *n2);


typedef void (*rbaugment_t)(rbnode_t *node);


typedef void (*rbdump_t)(rbnode_t *node);


typedef struct _rbtree_t {
	rbnode_t *root;
	rbcomp_t compare;
	rbaugment_t augment;
} rbtree_t;


extern void lib_rbInit(rbtree_t *tree, rbcomp_t compare, rbaugment_t augment);


extern int lib_rbInsert(rbtree_t *tree, rbnode_t *node);


extern void lib_rbRemove(rbtree_t *tree, rbnode_t *node);


extern rbnode_t *lib_rbMinimum(rbnode_t *node);


extern rbnode_t *lib_rbMaximum(rbnode_t *node);


extern rbnode_t *lib_rbPrev(rbnode_t *node);


extern rbnode_t *lib_rbNext(rbnode_t *node);


extern rbnode_t *lib_rbFind(rbtree_t *tree, rbnode_t *node);


extern rbnode_t *lib_rbFindEx(rbnode_t *root, rbnode_t *node, rbcomp_t compare);


extern void lib_rbDump(rbnode_t *node, rbdump_t dump);


#endif
