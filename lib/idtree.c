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

#include "lib.h"


static int lib_idtreeCmp(rbnode_t *n1, rbnode_t *n2)
{
	idnode_t *i1 = lib_treeof(idnode_t, linkage, n1);
	idnode_t *i2 = lib_treeof(idnode_t, linkage, n2);

	return i1->id - i2->id;
}


static void lib_idtreeAugment(rbnode_t *node)
{
	rbnode_t *it;
	idnode_t *n = lib_treeof(idnode_t, linkage, node);
	idnode_t *p = n, *r, *l;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(idnode_t, linkage, it->parent);
			if (it->parent->right == it) {
				break;
			}
		}

		n->lmaxgap = (n->id <= p->id) ? n->id : (n->id - p->id - 1);
	}
	else {
		l = lib_treeof(idnode_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(idnode_t, linkage, it->parent);
			if (it->parent->left == it) {
				break;
			}
		}

		n->rmaxgap = (n->id >= p->id) ? (MAX_ID - n->id - 1) : (p->id - n->id - 1);
	}
	else {
		r = lib_treeof(idnode_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(idnode_t, linkage, it);
		p = lib_treeof(idnode_t, linkage, it->parent);

		if (it->parent->left == it) {
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		}
		else {
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
		}
	}
}


idnode_t *lib_idtreeFind(idtree_t *tree, int id)
{
	idnode_t n;
	n.id = id;
	return lib_treeof(idnode_t, linkage, lib_rbFind(tree, &n.linkage));
}


idnode_t *lib_idtreeMinimum(rbnode_t *node)
{
	rbnode_t *x = node;

	if (x != NULL) {
		while (x->left != NULL) {
			x = x->left;
		}
	}

	return lib_treeof(idnode_t, linkage, x);
}


idnode_t *lib_idtreeNext(rbnode_t *node)
{
	rbnode_t *x = node;

	if (x->right != NULL) {
		return lib_treeof(idnode_t, linkage, lib_rbMinimum(x->right));
	}

	while ((x->parent != NULL) && (x == x->parent->right)) {
		x = x->parent;
	}

	return lib_treeof(idnode_t, linkage, x->parent);
}


int lib_idtreeInsert(idtree_t *tree, idnode_t *z)
{
	return lib_rbInsert((rbtree_t *)tree, &z->linkage);
}


void lib_idtreeRemove(idtree_t *tree, idnode_t *node)
{
	lib_rbRemove(tree, &node->linkage);
}


int lib_idtreeId(idnode_t *node)
{
	return node->id;
}


int lib_idtreeAlloc(idtree_t *tree, idnode_t *n, int min)
{
	idnode_t *f;

	if (min > MAX_ID) {
		return -1;
	}

	n->id = min;

	f = lib_idtreeFind(tree, min);
	if (f != NULL) {
		/* Go back until some space > min is found */
		while (f->rmaxgap == 0) {
			f = lib_treeof(idnode_t, linkage, f->linkage.parent);
			if (f == NULL) {
				/* Only id < min are available, fail */
				return -1;
			}
		}

		/* Got rmaxgap now */
		n->id = f->id + 1;

		/* Go right at least once so id > min */
		f = lib_treeof(idnode_t, linkage, f->linkage.right);

		/* Find minimal free space */
		while (f != NULL) {
			if (f->lmaxgap > 0) {
				n->id = f->id - 1;
				f = lib_treeof(idnode_t, linkage, f->linkage.left);
			}
			else {
				n->id = f->id + 1;
				f = lib_treeof(idnode_t, linkage, f->linkage.right);
			}
		}
	}

	LIB_ASSERT(lib_idtreeFind(tree, n->id) == NULL, "ID alloc failed - got existing ID %d", n->id);

	/* MISRA Rule 17.7: Unused return value, (void) added */
	(void)lib_rbInsert(tree, &n->linkage);

	return n->id;
}


static void _lib_idtreeDump(rbnode_t *node)
{
	idnode_t *n = lib_treeof(idnode_t, linkage, node);
	/* MISRA Rule 17.7: Unused return value, (void) added */
	(void)lib_printf("%d <0x%p>]", n->id, n);
}


void lib_idtreeDump(rbnode_t *node)
{
	lib_rbDump(node, _lib_idtreeDump);
}


void lib_idtreeInit(idtree_t *tree)
{
	lib_rbInit(tree, lib_idtreeCmp, lib_idtreeAugment);
}
