/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Min/max binary heap test
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "vm/vm.h"

#define NODE_CNT 50


typedef struct _test_data_t {
	bhnode_t linkage;
	struct _test_data_t *prev;
	struct _test_data_t *next;
	int key;
} test_data_t;


static struct {
	bheap_t heap;
	test_data_t *list;
} test_bhCommon;


static int test_bhCompare(bhnode_t *n1, bhnode_t *n2)
{
	test_data_t *d1 = lib_bhof(test_data_t, linkage, n1);
	test_data_t *d2 = lib_bhof(test_data_t, linkage, n2);

	return d2->key - d1->key;
}


static int test_bhCheckCondition(bhnode_t *node)
{
	if (node == NULL) {
		return 0;
	}

	if (node->left != NULL) {
		if (test_bhCompare(node, node->left) < 0) {
			return -1;
		}

		if (test_bhCheckCondition(node->left) < 0) {
			return -1;
		}
	}

	if (node->right != NULL) {
		if (test_bhCompare(node, node->right) < 0) {
			return -1;
		}

		if (test_bhCheckCondition(node->right) < 0) {
			return -1;
		}
	}

	return 0;
}


static int test_bhAddNode(int key)
{
	test_data_t *data = vm_kmalloc(sizeof(*data));
	if (data == NULL) {
		return -1;
	}

	data->key = key;
	lib_bhInsert(&test_bhCommon.heap, &data->linkage);
	LIST_ADD(&test_bhCommon.list, data);

	return 0;
}


static int test_bhRemoveNode(int key)
{
	test_data_t *curr = test_bhCommon.list;

	if (curr == NULL) {
		return -1;
	}

	do {
		if (curr->key == key) {
			LIST_REMOVE(&test_bhCommon.list, curr);
			lib_bhRemove(&test_bhCommon.heap, &curr->linkage);
			vm_kfree(curr);
			return 0;
		}
		curr = curr->next;
	} while (curr != test_bhCommon.list);

	return -1;
}


static void test_bhDump(bhnode_t *node)
{
	test_data_t *data = lib_bhof(test_data_t, linkage, node);

	lib_printf("%d", data->key);
}


void test_bh(void)
{
	int i;
	unsigned int seed = 5318008;

	lib_bhInit(&test_bhCommon.heap, test_bhCompare);

	lib_printf("test bh: Adding nodes ascending order\n");
	for (i = 0; i < NODE_CNT; ++i) {
		if (test_bhAddNode(i) < 0) {
			lib_printf("test bh: node add fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);

	lib_printf("test bh: Removing nodes ascending order\n");
	for (i = 0; i < NODE_CNT; ++i) {
		if (test_bhRemoveNode(i) < 0) {
			lib_printf("test bh: node remove fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);

	lib_printf("test bh: Adding nodes decreasing order\n");
	for (i = NODE_CNT - 1; i >= 0; --i) {
		if (test_bhAddNode(i) < 0) {
			lib_printf("test bh: node add fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);

	lib_printf("test bh: Removing nodes decreasing order\n");
	for (i = NODE_CNT - 1; i >= 0; --i) {
		if (test_bhRemoveNode(i) < 0) {
			lib_printf("test bh: node remove fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);

	lib_printf("test bh: Adding pseudo-random keys\n");
	for (i = 0; i < NODE_CNT; ++i) {
		if (test_bhAddNode(lib_rand(&seed) % 32768) < 0) {
			lib_printf("test bh: node add fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);

	lib_printf("test bh: Removing pseudo-random keys\n");
	while (test_bhCommon.list != NULL) {
		if (test_bhRemoveNode(test_bhCommon.list->key) < 0) {
			lib_printf("test bh: node remove fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);

	lib_printf("test bh: Adding identical key\n");
	for (i = NODE_CNT - 1; i >= 0; --i) {
		if (test_bhAddNode(420) < 0) {
			lib_printf("test bh: node add fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);

	lib_printf("test bh: cleanup\n");
	for (i = NODE_CNT - 1; i >= 0; --i) {
		if (test_bhRemoveNode(420) < 0) {
			lib_printf("test bh: node remove fail\n");
		}
		if (test_bhCheckCondition(test_bhCommon.heap.root) < 0) {
			lib_printf("test bh: Heap is damaged!\n");
			lib_bhDump(test_bhCommon.heap.root, test_bhDump);
			return;
		}
	}

	lib_bhDump(test_bhCommon.heap.root, test_bhDump);
}
