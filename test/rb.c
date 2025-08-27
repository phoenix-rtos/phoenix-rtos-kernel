/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for red-black tree
 *
 * Copyright 2017 Phoenix Systems
 * Author: Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* parasoft-begin-suppress ALL "tests don't need to comply with MISRA" */

#include "hal/hal.h"
#include "lib/lib.h"
#include "vm/vm.h"
#include "proc/proc.h"


static int test_rbCheckEx(rbnode_t *node, int level)
{
	if (node == NULL) {
		return 1;
	}

	if (node->color == RB_RED) {
		if ((node->left != NULL && node->left->color == RB_RED) ||
				(node->right != NULL && node->right->color == RB_RED)) {
			return -1;
		}
	}

	int left = test_rbCheckEx(node->left, level + 1);
	int right = test_rbCheckEx(node->right, level + 1);

	if (left == -1 || right == -1) {
		return -1;
	}

	if (left != right) {
		return -1;
	}

	return left + ((node->color == RB_BLACK) ? 1 : 0);
}


static int test_rbCheck(rbtree_t *tree)
{
	if (tree->root == NULL) {
		return 0;
	}

	if (tree->root->color != RB_BLACK) {
		return -1;
	}

	if (test_rbCheckEx(tree->root, 0) < 0) {
		return -1;
	}

	return 0;
}


typedef struct _test_t {
	rbnode_t node;

	int num;
} test_t;


static int test_compare(rbnode_t *n1, rbnode_t *n2)
{
	test_t *t1 = lib_treeof(test_t, node, n1);
	test_t *t2 = lib_treeof(test_t, node, n2);

	if (t1->num == t2->num) {
		return 0;
	}

	return (t1->num > t2->num) ? 1 : -1;
}


#define RB_TEST_SIZE 7
int count;


static int rb_processVector(int insert, rbtree_t *tree, int vector[])
{
	test_t *test;
	int i;

	for (i = 0; i < RB_TEST_SIZE; ++i) {
		if (insert) {
			test = vm_kmalloc(sizeof(test_t));
			test->num = vector[i];

			/* MISRA Rule 17.7: Unused return value, (void) added */
			(void)lib_rbInsert(tree, &test->node);
		}
		else {
			test_t t;
			t.num = vector[i];

			test_t *to_remove = lib_treeof(test_t, node, lib_rbFind(tree, &t.node));
			if (to_remove == NULL) {
				return -1;
			}

			lib_rbRemove(tree, &to_remove->node);
			vm_kfree(to_remove);
		}
	}

	return test_rbCheck(tree);
}


static void test_rbGenerateTest(int level, int insert, int vector[], int selected[], int input[])
{
	int i, j;
	for (i = 0; i < RB_TEST_SIZE; ++i) {
		if (selected[i]) {
			continue;
		}

		selected[i] = 1;
		vector[level] = i + 1;

		if (level != (RB_TEST_SIZE - 1)) {
			test_rbGenerateTest(level + 1, insert, vector, selected, input);
		}
		else {
			if (insert) {
				int remove_vector[RB_TEST_SIZE] = { 0 };
				int remove_selected[RB_TEST_SIZE] = { 0 };

				test_rbGenerateTest(0, 0, remove_vector, remove_selected, vector);
			}
			else {
				rbtree_t tree;
				lib_rbInit(&tree, test_compare, NULL);

				++count;

				if (rb_processVector(1, &tree, input) < 0) {
					/* MISRA Rule 17.7: Unused return value, (void) added in lines 152, 154, 157, 162, 164, 167*/
					(void)lib_printf("error: RB insert - ");
					for (j = 0; j < RB_TEST_SIZE; ++j) {
						(void)lib_printf("%d ", input[j]);
					}

					(void)lib_printf("\n");
					hal_cpuHalt();
				}

				if (rb_processVector(0, &tree, vector) < 0) {
					(void)lib_printf("error: RB remove - ");
					for (j = 0; j < RB_TEST_SIZE; ++j) {
						(void)lib_printf("%d ", vector[j]);
					}

					(void)lib_printf("\n");
					hal_cpuHalt();
				}
			}
		}

		selected[i] = 0;
	}
}


static void test_rb_autothr(void *arg)
{
	int vector[RB_TEST_SIZE];
	int selected[RB_TEST_SIZE];

	/* MISRA Rule 17.7: Unused return value, (void) added in lines 184, 194, 201*/
	(void)lib_printf("test: Start automatic red-black tree test\n");

	for (;;) {
		count = 0;

		hal_memset(vector, 0, sizeof(vector));
		hal_memset(selected, 0, sizeof(selected));

		test_rbGenerateTest(0, 1, vector, selected, NULL);

		(void)lib_printf("success: RB test vector size: %d, test count: %d\n", RB_TEST_SIZE, count);
	}
}


void test_rb(void)
{
	(void)proc_threadCreate(NULL, test_rb_autothr, NULL, 1, 512, NULL, 0, NULL);
}

/* parasoft-end-suppress ALL "tests don't need to comply with MISRA" */
