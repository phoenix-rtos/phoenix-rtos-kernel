#ifndef _LIB_ID_H_
#define _LIB_ID_H_

#include "rb.h"

#define LIMIT_ID (1 << 30)


typedef struct _idnode_t {
	rbnode_t linkage;
	unsigned id : 30;
	unsigned lgap : 1;
	unsigned rgap : 1;
} idnode_t;


#define lib_idof(type, node_field, node) ({					\
	long _off = (long) &(((type *) 0)->node_field);				\
	idnode_t *tmpnode = (node);					\
	(type *) ((tmpnode == NULL) ? NULL : ((void *) tmpnode - _off));	\
})


typedef struct _idtree_t {
	rbtree_t rb;
	unsigned next;
} idtree_t;


int lib_idAlloc(idtree_t *tree, idnode_t *node);


idnode_t *lib_idFind(idtree_t *tree, unsigned id);


void lib_idRemove(idtree_t *tree, idnode_t *node);


void lib_idInit(idtree_t *tree);


#endif
