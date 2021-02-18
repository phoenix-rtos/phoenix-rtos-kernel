#include "lib/lib.h"
#include "test/test.h"

static int cmp_gt(void* lhs, void* rhs)
{
	int l = *(int*)lhs, r = *(int*)rhs;
	if (l == r)
		return 0;
	else if (l > r)
		return 1;
	else
		return -1;
}

static int cmp_lt(void* lhs, void* rhs)
{
	int l = *(int*)lhs, r = *(int*)rhs;
	if (l == r)
		return 0;
	else if (l < r)
		return 1;
	else
		return -1;
}

TEST_GROUP(test_bsearch);

TEST_SETUP(test_bsearch)
{
}

TEST_TEAR_DOWN(test_bsearch)
{
}

TEST(test_bsearch, basic)
{
	int arr_inc[] = {1, 2, 3, 4, 5, 6, 7, 8};
	int arr_dec[] = {7, 6, 5, 4, 3, 2, 1};
	int arr_single[] = {1};

	for (int key = 1; key < 8; key++) {
		void *res = lib_bsearch(&key, arr_inc, 8, sizeof(int), cmp_gt);
		TEST_ASSERT_EQUAL_PTR(&arr_inc[key-1], res);
	}

	for (int key = 7; key > 1; key--) {
		void *res = lib_bsearch(&key, arr_dec, 7, sizeof(int), cmp_lt);
		TEST_ASSERT_EQUAL_PTR(&arr_dec[7-key], res);
	}

	TEST_ASSERT_EQUAL_PTR(
		&arr_single[0],
		lib_bsearch(&arr_single[0], arr_single, 1, sizeof(int), cmp_gt)
	);
}

TEST(test_bsearch, first_matching)
{
	int arr[] = {0, 0, 0};
	int key = 0;

	TEST_ASSERT_EQUAL_PTR(&arr[1], lib_bsearch(&key, arr, 3, sizeof(int), cmp_gt));
	TEST_ASSERT_EQUAL_PTR(&arr[1], lib_bsearch(&key, arr, 3, sizeof(int), cmp_lt));
}

TEST(test_bsearch, not_found)
{
	int arr[] = {1, 2, 4, 5};
	int key = 0;

	TEST_ASSERT_EQUAL_PTR(NULL, lib_bsearch(&key, arr, 4, sizeof(int), cmp_gt));
}

TEST_GROUP_RUNNER(test_bsearch)
{
	RUN_TEST_CASE(test_bsearch, basic);
	RUN_TEST_CASE(test_bsearch, first_matching);
	RUN_TEST_CASE(test_bsearch, not_found);
}
