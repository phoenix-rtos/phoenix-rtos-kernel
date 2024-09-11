/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Min/max binary heap
 *
 * Copyright 2017, 2024 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "hal/hal.h"
#include "include/errno.h"
#include "lib.h"


static void lib_bhSwapPtr(bhnode_t **p1, bhnode_t **p2)
{
	bhnode_t *t = *p1;
	*p1 = *p2;
	*p2 = t;
}


static void lib_bhAttachChildren(bhnode_t *node)
{
	if (node->left != NULL) {
		node->left->parent = node;
	}
	if (node->right != NULL) {
		node->right->parent = node;
	}
}


static void lib_bhSwap(bheap_t *heap, bhnode_t *n1, bhnode_t *n2)
{
	/* Turn two symmetrical edge cases into one */
	if (n2->parent == n1) {
		lib_bhSwapPtr(&n1, &n2);
	}

	/* Swap all linkages */
	lib_bhSwapPtr(&n1->left, &n2->left);
	lib_bhSwapPtr(&n1->right, &n2->right);
	lib_bhSwapPtr(&n1->parent, &n2->parent);

	/* Handle parent-child edge case */
	if (n2->parent == n2) {
		n2->parent = n1;

		if (n1->left == n1) {
			n1->left = n2;
		}
		else {
			n1->right = n2;
		}
	}

	/* Handle siblings edge case or fix parents */
	if (n1->parent == n2->parent) {
		lib_bhSwapPtr(&n1->parent->left, &n2->parent->right);
	}
	else {
		if (n1->parent != NULL) {
			if (n1->parent->left == n2) {
				n1->parent->left = n1;
			}
			else if (n1->parent->right == n2) {
				n1->parent->right = n1;
			}
		}

		if (n2->parent != NULL) {
			if (n2->parent->left == n1) {
				n2->parent->left = n2;
			}
			else if (n2->parent->right == n2) {
				n2->parent->right = n2;
			}
		}
	}

	/* Set children parent */
	lib_bhAttachChildren(n1);
	lib_bhAttachChildren(n2);

	/* Fix global pointers */
	if (heap->root == n1) {
		heap->root = n2;
	}
	else if (heap->root == n2) {
		heap->root = n1;
	}

	if (heap->tail == n1) {
		heap->tail = n2;
	}
	else if (heap->tail == n2) {
		heap->tail = n1;
	}
}


static void lib_bhHeapify(bheap_t *heap, bhnode_t *node)
{
	bhnode_t *parent = node->parent, *n = node;

	while ((parent != NULL) && (heap->comp(n, parent) > 0)) {
		lib_bhSwap(heap, n, parent);
		parent = n->parent;
	}
}


static void lib_bhRevHeapify(bheap_t *heap, bhnode_t *node)
{
	bhnode_t *n = node, *min;

	while ((n != NULL) && (n->left != NULL)) {
		min = n->left;
		if ((n->right != NULL) && (heap->comp(n->right, min) > 0)) {
			min = n->right;
		}
		if (heap->comp(n, min) >= 0) {
			break;
		}

		lib_bhSwap(heap, n, min);
	}
}


static void lib_bhNextTail(bheap_t *heap)
{
	bhnode_t *t = heap->tail;

	for (;;) {
		if (t->parent == NULL) {
			heap->tail = t;
			while (heap->tail->left != NULL) {
				heap->tail = heap->tail->left;
			}
			break;
		}
		else if (t->parent->left == t) {
			heap->tail = t->parent->right;
			while (heap->tail->left != NULL) {
				heap->tail = heap->tail->left;
			}
			break;
		}

		t = t->parent;
	}
}


static bhnode_t *lib_bhPrevTail(bheap_t *heap)
{
	bhnode_t *prev = heap->tail;

	while ((prev->parent != NULL) && (prev->parent->left == prev)) {
		prev = prev->parent;
	}

	if (prev->parent != NULL) {
		prev = prev->parent->left;
	}

	while (prev->right != NULL) {
		prev = prev->right;
	}

	return prev;
}


void lib_bhInsert(bheap_t *heap, bhnode_t *node)
{
	if (heap->root == NULL) {
		heap->root = node;
		heap->tail = node;
		node->parent = NULL;
		node->left = NULL;
		node->right = NULL;
	}
	else if (heap->tail->left == NULL) {
		heap->tail->left = node;
		node->parent = heap->tail;
		node->left = NULL;
		node->right = NULL;
		lib_bhHeapify(heap, node);
	}
	else {
		heap->tail->right = node;
		node->parent = heap->tail;
		node->left = NULL;
		node->right = NULL;
		lib_bhHeapify(heap, node);
		lib_bhNextTail(heap);
	}
}


void lib_bhRemove(bheap_t *heap, bhnode_t *node)
{
	bhnode_t *prev, *swap;

	for (;;) {
		if (heap->tail->right != NULL) {
			if (heap->tail->right == node) {
				heap->tail->right = NULL;
				break;
			}

			swap = heap->tail->right;
			lib_bhSwap(heap, swap, node);
			heap->tail->right = NULL;

			if (heap->comp(node, swap) < 0) {
				lib_bhHeapify(heap, swap);
			}
			else {
				lib_bhRevHeapify(heap, swap);
			}
			break;
		}
		else if (heap->tail->left != NULL) {
			if (heap->tail->left == node) {
				heap->tail->left = NULL;
				break;
			}

			swap = heap->tail->left;
			lib_bhSwap(heap, swap, node);
			heap->tail->left = NULL;

			if (heap->comp(node, swap) < 0) {
				lib_bhHeapify(heap, swap);
			}
			else {
				lib_bhRevHeapify(heap, swap);
			}
			break;
		}
		else if (heap->tail == heap->root) {
			heap->tail = NULL;
			heap->root = NULL;
			break;
		}

		prev = lib_bhPrevTail(heap);

		if ((prev->left == NULL) && (prev->right == NULL)) {
			/* Special case - current tail is actually the newest node */
			heap->tail = prev->parent;
		}
		else {
			heap->tail = prev;
		}
	}
}


bhnode_t *lib_bhPop(bheap_t *heap)
{
	bhnode_t *ret = heap->root;

	if (heap->root != NULL) {
		lib_bhRemove(heap, heap->root);
	}

	return ret;
}


bhnode_t *lib_bhPeek(bheap_t *heap)
{
	return heap->root;
}


#define BH_DUMP_DEPTH 16


static void lib_bhDumpEx(bhnode_t *node, bhdump_t dump, unsigned int *depth, unsigned char d[BH_DUMP_DEPTH])
{
	unsigned int i;

	for (i = 0; i < *depth; i++) {
		lib_printf("%c ", d[i] ? '|' : ' ');
	}

	if (node == NULL) {
		lib_printf("%s() *\n", *depth ? "`-" : "");
		return;
	}

	lib_printf("%s(", *depth ? "`-" : "");
	dump(node);
	lib_printf(")\n");

	(*depth)++;
	if ((node->left != NULL) || (node->right != NULL)) {
		if (*depth < BH_DUMP_DEPTH) {
			d[*depth] = 1;
			lib_bhDumpEx(node->left, dump, depth, d);
			d[*depth] = 0;
			lib_bhDumpEx(node->right, dump, depth, d);
		}
		else {
			for (i = 0; i < *depth; i++) {
				lib_printf("%c ", d[i] ? '|' : ' ');
			}

			lib_printf("%s(..)\n", *depth ? "`-" : "");
		}
	}
	(*depth)--;
}


void lib_bhDump(bhnode_t *node, bhdump_t dump)
{
	unsigned int depth = 0;
	unsigned char d[BH_DUMP_DEPTH];

	hal_memset(d, 0, BH_DUMP_DEPTH);
	lib_bhDumpEx(node, dump, &depth, d);
}


void lib_bhInit(bheap_t *heap, bhcomp_t compare)
{
	heap->root = NULL;
	heap->tail = NULL;
	heap->comp = compare;
}
