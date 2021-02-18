#include "lib/lib.h"
#include "test/test.h"

struct node {
	struct node *next;
	struct node *prev;
	int val;
};

TEST_GROUP(test_list);

TEST_SETUP(test_list)
{
}

TEST_TEAR_DOWN(test_list)
{
}

TEST(test_list, basic_add_remove)
{
	struct node *head = NULL;
	struct node node[2] = {
		{
			.next = NULL,
			.prev = NULL,
			.val = 0
		},
		{
			.next = NULL,
			.prev = NULL,
			.val = 1
		}
	};

	LIST_ADD(&head, &node[0]);

	TEST_ASSERT_EQUAL_PTR(head, &node[0]);
	TEST_ASSERT_EQUAL_PTR(&node[0], node[0].next);
	TEST_ASSERT_EQUAL_PTR(&node[0], node[0].prev);
	TEST_ASSERT_EQUAL_INT(node[0].val, 0);

	LIST_ADD(&head, &node[1]);

	TEST_ASSERT_EQUAL_PTR(head, &node[0]);
	TEST_ASSERT_EQUAL_PTR(head->next, &node[1]); 
	TEST_ASSERT_EQUAL_PTR(head->prev, &node[1]); 
	TEST_ASSERT_EQUAL_PTR(node[1].next, head);
	TEST_ASSERT_EQUAL_PTR(node[1].prev, head);
	TEST_ASSERT_EQUAL_INT(node[1].val, 1);

	// Remove head
	LIST_REMOVE(&head, &node[0]);

	TEST_ASSERT_EQUAL_PTR(head, &node[1]);
	TEST_ASSERT_EQUAL_PTR(&node[1], node[1].next);
	TEST_ASSERT_EQUAL_PTR(&node[1], node[1].prev);
	TEST_ASSERT_EQUAL_INT(node[1].val, 1);
	TEST_ASSERT_EQUAL_PTR(node[0].next, NULL);
	TEST_ASSERT_EQUAL_PTR(node[0].prev, NULL);
	TEST_ASSERT_EQUAL_INT(node[0].val, 0);

	// Add one more time node no. 0
	LIST_ADD(&head, &node[0]);

	TEST_ASSERT_EQUAL_PTR(head, &node[1]);
	TEST_ASSERT_EQUAL_PTR(head->next, &node[0]); 
	TEST_ASSERT_EQUAL_PTR(head->prev, &node[0]); 
	TEST_ASSERT_EQUAL_PTR(node[0].next, head);
	TEST_ASSERT_EQUAL_PTR(node[0].prev, head);
	TEST_ASSERT_EQUAL_INT(node[0].val, 0);

	// Remove tail
	LIST_REMOVE(&head, &node[0]);

	TEST_ASSERT_EQUAL_PTR(head, &node[1]);
	TEST_ASSERT_EQUAL_PTR(&node[1], node[1].next);
	TEST_ASSERT_EQUAL_PTR(&node[1], node[1].prev);
	TEST_ASSERT_EQUAL_INT(node[1].val, 1);
	TEST_ASSERT_EQUAL_PTR(node[0].next, NULL);
	TEST_ASSERT_EQUAL_PTR(node[0].prev, NULL);
	TEST_ASSERT_EQUAL_INT(node[0].val, 0);

	// Remove head
	LIST_REMOVE(&head, head);

	TEST_ASSERT_EQUAL_PTR(head, NULL);
	TEST_ASSERT_EQUAL_PTR(node[1].next, NULL);
	TEST_ASSERT_EQUAL_PTR(node[1].prev, NULL);
	TEST_ASSERT_EQUAL_PTR(node[1].val, 1);
}

TEST(test_list, add_null_has_no_effect)
{
	struct node *head = NULL;
	struct node node = {
		.next = NULL,
		.prev = NULL,
		.val = 0xDA
	};

	lib_listAdd((void**)&head, NULL, 0, 0);

	TEST_ASSERT_EQUAL_PTR(head, NULL);

	LIST_ADD(&head, &node);

	TEST_ASSERT_EQUAL_PTR(head, &node);
	TEST_ASSERT_EQUAL_PTR(&node, node.next);
	TEST_ASSERT_EQUAL_PTR(&node, node.prev);
	TEST_ASSERT_EQUAL_INT(node.val, 0xDA);

	lib_listAdd((void**)&head, NULL, 0, 0);

	TEST_ASSERT_EQUAL_PTR(head, &node);
	TEST_ASSERT_EQUAL_PTR(&node, node.next);
	TEST_ASSERT_EQUAL_PTR(&node, node.prev);
	TEST_ASSERT_EQUAL_INT(node.val, 0xDA);

	lib_listAdd(NULL, NULL, 0, 0);
}

TEST(test_list, remove_null_has_no_effect)
{
	struct node *head = NULL;
	struct node node = {
		.next = NULL,
		.prev = NULL,
		.val = 0xDA
	};

	LIST_ADD(&head, &node);

	TEST_ASSERT_EQUAL_PTR(head, &node);
	TEST_ASSERT_EQUAL_PTR(&node, node.next);
	TEST_ASSERT_EQUAL_PTR(&node, node.prev);
	TEST_ASSERT_EQUAL_INT(node.val, 0xDA);

	lib_listRemove((void**)&head, NULL, 0, 0);

	TEST_ASSERT_EQUAL_PTR(head, &node);
	TEST_ASSERT_EQUAL_PTR(&node, node.next);
	TEST_ASSERT_EQUAL_PTR(&node, node.prev);
	TEST_ASSERT_EQUAL_INT(node.val, 0xDA);

	lib_listRemove(NULL, NULL, 0, 0);
}

TEST_GROUP_RUNNER(test_list)
{
	RUN_TEST_CASE(test_list, basic_add_remove);
	RUN_TEST_CASE(test_list, add_null_has_no_effect);
	RUN_TEST_CASE(test_list, remove_null_has_no_effect);
}
