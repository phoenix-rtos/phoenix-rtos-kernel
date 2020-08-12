/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - RB_RED-RB_BLACK tree
 *
 * Copyright 2017 Phoenix Systems
 * Author: Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../include/errno.h"
#include "lib.h"


void lib_rbInit(rbtree_t *tree, rbcomp_t compare, rbaugment_t augment)
{
	tree->root = NULL;
	tree->compare = compare;
	tree->augment = augment;
}


static inline void rb_augment(rbtree_t *tree, rbnode_t *node)
{
	if (node == NULL)
		return;

	if (tree->augment != NULL)
		tree->augment(node);
}


static void rb_rotateLeft(rbtree_t *tree, rbnode_t *x)
{
	rbnode_t *y = x->right;
	x->right = y->left;

	if (y->left != NULL)
		y->left->parent = x;

	y->parent = x->parent;
	if (x->parent == NULL)
		tree->root = y;
	else if (x == x->parent->left)
		x->parent->left = y;
	else
		x->parent->right = y;

	y->left = x;
	x->parent = y;

	rb_augment(tree, y->left);
	rb_augment(tree, y->right);
}


static void rb_rotateRight(rbtree_t *tree, rbnode_t *x)
{
	rbnode_t *y = x->left;
	x->left = y->right;

	if (y->right != NULL)
		y->right->parent = x;

	y->parent = x->parent;
	if (x->parent == NULL)
		tree->root = y;
	else if (x == x->parent->right)
		x->parent->right = y;
	else
		x->parent->left = y;

	y->right = x;
	x->parent = y;

	rb_augment(tree, y->left);
	rb_augment(tree, y->right);
}


static void lib_rbInsertBalance(rbtree_t *tree, rbnode_t *node)
{
	rbnode_t *z = node;
	rbnode_t *y;

	while (z->parent != NULL && z->parent->color == RB_RED) {
		if (z->parent == z->parent->parent->left) {
			y = z->parent->parent->right;
			if (y != NULL && y->color == RB_RED) {
				z->parent->color = RB_BLACK;
				y->color = RB_BLACK;
				z->parent->parent->color = RB_RED;
				z = z->parent->parent;
			}
			else if (z == z->parent->right) {
				z = z->parent;
				rb_rotateLeft(tree, z);
			}
			else {
				z->parent->color = RB_BLACK;
				z->parent->parent->color = RB_RED;
				rb_rotateRight(tree, z->parent->parent);
			}
		}
		else {
			y = z->parent->parent->left;
			if (y != NULL && y->color == RB_RED) {
				z->parent->color = RB_BLACK;
				y->color = RB_BLACK;
				z->parent->parent->color = RB_RED;
				z = z->parent->parent;
			}
			else if (z == z->parent->left) {
				z = z->parent;
				rb_rotateRight(tree, z);
			}
			else {
				z->parent->color = RB_BLACK;
				z->parent->parent->color = RB_RED;
				rb_rotateLeft(tree, z->parent->parent);
			}
		}
	}

	tree->root->color = RB_BLACK;
}


static void lib_rbRemoveBalance(rbtree_t *tree, rbnode_t *parent, rbnode_t *node)
{
	rbnode_t nil = {
		.color = RB_BLACK,
		.parent = parent,
	};

	rbnode_t *x = (node == NULL) ? &nil : node;
	rbnode_t *w;

	if (tree->root == NULL)
		return;

	while (x != tree->root && x->color == RB_BLACK) {
		if (x == x->parent->left || (x == &nil && x->parent->left == NULL)) {
			w = x->parent->right;

			if (w->color == RB_RED) {
				w->color = RB_BLACK;
				x->parent->color = RB_RED;
				rb_rotateLeft(tree, x->parent);
				w = x->parent->right;
			}

			if ((w->left == NULL || w->left->color == RB_BLACK) &&
			    (w->right == NULL || w->right->color == RB_BLACK)) {
				w->color = RB_RED;
				x = x->parent;
			}
			else if (w->right == NULL || w->right->color == RB_BLACK) {
				w->left->color = RB_BLACK;
				w->color = RB_RED;
				rb_rotateRight(tree, w);
				w = x->parent->right;
			}
			else {
				w->color = x->parent->color;
				x->parent->color = RB_BLACK;
				w->right->color = RB_BLACK;
				rb_rotateLeft(tree, x->parent);
				x = tree->root;
			}
		}
		else {
			w = x->parent->left;

			if (w->color == RB_RED) {
				w->color = RB_BLACK;
				x->parent->color = RB_RED;
				rb_rotateRight(tree, x->parent);
				w = x->parent->left;
			}

			if ((w->right == NULL || w->right->color == RB_BLACK) &&
			    (w->left == NULL || w->left->color == RB_BLACK)) {
				w->color = RB_RED;
				x = x->parent;
			}
			else if (w->left == NULL || w->left->color == RB_BLACK) {
				w->right->color = RB_BLACK;
				w->color = RB_RED;
				rb_rotateLeft(tree, w);
				w = x->parent->left;
			}
			else {
				w->color = x->parent->color;
				x->parent->color = RB_BLACK;
				w->left->color = RB_BLACK;
				rb_rotateRight(tree, x->parent);
				x = tree->root;
			}
		}
	}

	x->color = RB_BLACK;
}


static void rb_transplant(rbtree_t *tree, rbnode_t *u, rbnode_t *v)
{
	if (u->parent != NULL) {
		if (u == u->parent->left)
			u->parent->left = v;
		else
			u->parent->right = v;

		rb_augment(tree, u->parent);
	}
	else
		tree->root = v;

	if (v != NULL)
		v->parent = u->parent;

	rb_augment(tree, v);
}


int lib_rbInsert(rbtree_t *tree, rbnode_t *z)
{
	rbnode_t *y = NULL;
	rbnode_t *x = tree->root;
	int c = 0;

	while (x != NULL) {
		y = x;

		c = tree->compare(y, z);
		if (c == 0)
			return -EEXIST;

		x = (c > 0) ? x->left : x->right;
	}

	z->parent = y;
	if (y == NULL)
		tree->root = z;
	else if (c > 0)
		y->left = z;
	else
		y->right = z;

	z->left = NULL;
	z->right = NULL;
	z->color = RB_RED;

	rb_augment(tree, z);
	lib_rbInsertBalance(tree, z);
	return EOK;
}


void lib_rbRemove(rbtree_t *tree, rbnode_t *z)
{
	rbnode_t *y = z;
	rbnode_t *x;
	rbnode_t *p = z->parent;
	rbnode_t *t;
	rbcolor_t c = y->color;

	if (z->left == NULL) {
		x = z->right;
		rb_transplant(tree, z, z->right);
	}
	else if (z->right == NULL) {
		x = z->left;
		rb_transplant(tree, z, z->left);
	}
	else {
		y = lib_rbMinimum(z->right);
		c = y->color;
		x = y->right;

		if (y->parent == z)
			p = y;
		else {
			p = y->parent;

			rb_transplant(tree, y, y->right);
			y->right = z->right;
			y->right->parent = y;
		}

		rb_transplant(tree, z, y);
		y->left = z->left;
		y->left->parent = y;
		y->color = z->color;

		t = lib_rbMaximum(y->left);
		rb_augment(tree, t);

		t = lib_rbMinimum(y->right);
		rb_augment(tree, t);
	}

	if (c == RB_BLACK)
		lib_rbRemoveBalance(tree, p, x);
}


rbnode_t *lib_rbMinimum(rbnode_t *node)
{
	rbnode_t *x = node;

	if (x == NULL)
		return x;

	while (x->left != NULL)
		x = x->left;

	return x;
}


rbnode_t *lib_rbMaximum(rbnode_t *node)
{
	rbnode_t *x = node;

	if (x == NULL)
		return x;

	while (x->right != NULL)
		x = x->right;

	return x;
}


rbnode_t *lib_rbPrev(rbnode_t *node)
{
	rbnode_t *x = node;

	if (x->left != NULL)
		return lib_rbMaximum(x->left);

	while (x->parent != NULL && x == x->parent->left)
		x = x->parent;

	return x->parent;
}


rbnode_t *lib_rbNext(rbnode_t *node)
{
	rbnode_t *x = node;

	if (x->right != NULL)
		return lib_rbMinimum(x->right);

	while (x->parent != NULL && x == x->parent->right)
		x = x->parent;

	return x->parent;
}


rbnode_t *lib_rbFind(rbtree_t *tree, rbnode_t *node)
{
	return lib_rbFindEx(tree->root, node, tree->compare);
}


rbnode_t *lib_rbFindEx(rbnode_t *root, rbnode_t *node, rbcomp_t compare)
{
	rbnode_t *it = root;
	int c;

	while (it != NULL) {
		c = compare(it, node);
		if (c == 0)
			return it;

		it = (c > 0) ? it->left : it->right;
	}

	return NULL;
}


#define RB_DUMP_DEPTH	16


void lib_rbDumpEx(rbnode_t *node, rbdump_t dump, unsigned int *depth, unsigned char d[RB_DUMP_DEPTH])
{
	unsigned int i;

	for (i = 0; i < *depth; i++)
		lib_printf("%c ", d[i] ? '|' : ' ');

	if (node == NULL) {
		lib_printf("%s() *\n", *depth ? "`-" : "");
		return;
	}

	lib_printf("%s(", *depth ? "`-" : "");
	dump(node);
	lib_printf(")%c\n", node->color == RB_BLACK ? '*' : ' ');

	(*depth)++;
	if ((node->left != NULL) || (node->right != NULL)) {

		if (*depth < RB_DUMP_DEPTH) {
			d[*depth] = 1;
			lib_rbDumpEx(node->left, dump, depth, d);
			d[*depth] = 0;
			lib_rbDumpEx(node->right, dump, depth, d);
		}
		else {
			for (i = 0; i < *depth; i++)
				lib_printf("%c ", d[i] ? '|' : ' ');

			lib_printf("%s(..)\n", *depth ? "`-" : "");
		}
	}
	(*depth)--;
}


void lib_rbDump(rbnode_t *node, rbdump_t dump)
{
	unsigned int depth = 0;
	unsigned char d[RB_DUMP_DEPTH];

	hal_memset(d, 0, RB_DUMP_DEPTH);
	lib_rbDumpEx(node, dump, &depth, d);
}
