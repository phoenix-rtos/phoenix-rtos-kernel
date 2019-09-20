#include "../../include/errno.h"
#include "lib.h"


static int id_alloc(idtree_t *tree, int id)
{
	idnode_t *p = lib_treeof(idnode_t, linkage, tree->rb.root);

	while (p != NULL) {
		if (p->lgap && id < p->id) {
			if (p->linkage.left == NULL)
				return id;

			p = lib_treeof(idnode_t, linkage, p->linkage.left);
			continue;
		}

		if (p->rgap) {
			if (p->linkage.right == NULL)
				return max(id, p->id + 1);

			if (id < p->id)
				id = p->id + 1;

			p = lib_treeof(idnode_t, linkage, p->linkage.right);

			continue;
		}

		for (;; p = lib_treeof(idnode_t, linkage, p->linkage.parent)) {
			if (p->linkage.parent == NULL)
				return -EAGAIN;

			if ((&p->linkage == p->linkage.parent->left) && lib_treeof(idnode_t, linkage, p->linkage.parent)->rgap)
				break;
		}

		p = lib_treeof(idnode_t, linkage, p->linkage.parent);

		if (p->linkage.right == NULL)
			return p->id + 1;

		if (id < p->id)
			id = p->id + 1;

		p = lib_treeof(idnode_t, linkage, p->linkage.right);
	}

	return id;
}


static int id_cmp(rbnode_t *n1, rbnode_t *n2)
{
	idnode_t *e1 = lib_treeof(idnode_t, linkage, n1);
	idnode_t *e2 = lib_treeof(idnode_t, linkage, n2);

	return (e1->id > e2->id) - (e1->id < e2->id);
}


static void id_augment(rbnode_t *node)
{
	rbnode_t *it;
	idnode_t *n = lib_treeof(idnode_t, linkage, node);
	idnode_t *p, *r, *l;

	if (node->left == NULL) {
		l = lib_treeof(idnode_t, linkage, lib_rbPrev(node));
		n->lgap = ((l == NULL) ? n->id : n->id - l->id - 1) != 0;
	}
	else {
		l = lib_treeof(idnode_t, linkage, node->left);
		n->lgap = l->lgap || l->rgap;
	}

	if (node->right == NULL) {
		r = lib_treeof(idnode_t, linkage, lib_rbNext(node));
		n->lgap = ((r == NULL) ? LIMIT_ID - n->id - 1 : r->id - n->id - 1) != 0;
	}
	else {
		r = lib_treeof(idnode_t, linkage, node->right);
		n->lgap = r->lgap || r->rgap;
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(idnode_t, linkage, it);
		p = lib_treeof(idnode_t, linkage, it->parent);

		if (it->parent->left == it)
			p->lgap = n->lgap || n->rgap;
		else
			p->rgap = n->lgap || n->rgap;
	}
}


int lib_idAlloc(idtree_t *tree, idnode_t *node)
{
	int id;

	id = id_alloc(tree, tree->next++);

	if (id < 0)
		id = id_alloc(tree, tree->next = 0);

	else if (tree->next == LIMIT_ID)
		tree->next = 0;

	if (id >= 0) {
		node->id = (unsigned)id;
		lib_rbInsert(&tree->rb, &node->linkage);
	}

	return id;
}


idnode_t *lib_idFind(idtree_t *tree, unsigned id)
{
	idnode_t t;
	t.id = id;
	return lib_treeof(idnode_t, linkage, lib_rbFind(&tree->rb, &t.linkage));
}


void lib_idRemove(idtree_t *tree, idnode_t *node)
{

}


void lib_idInit(idtree_t *tree)
{
	tree->next = 0;
	lib_rbInit(&tree->rb, id_cmp, id_augment);
}
