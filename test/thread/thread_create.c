#include "include/errno.h"
#include "proc/threads.h"
#include "test/test.h"

static int arg = 1337;
static unsigned int create_prio;

TEST_GROUP(test_thread_spawn);

TEST_SETUP(test_thread_spawn)
{
}

TEST_TEAR_DOWN(test_thread_spawn)
{
}

static void thread_end(void *args)
{
	proc_threadEnd();
	FAIL("thread didn't end");
}

TEST(test_thread_spawn, thread_end)
{
	int res;
	unsigned int p = proc_current()->priority;

	res = proc_threadCreate(NULL, thread_end, NULL, p, 1024, NULL, 0, NULL);
	TEST_ASSERT_EQUAL_INT(EOK, res);

	proc_reap();
}

static void thread_check_args(void *args)
{
	TEST_ASSERT_EQUAL_INT(arg, (int)args);
	proc_threadEnd();
}

TEST(test_thread_spawn, passing_arguments)
{
	int res;
	unsigned int p = proc_current()->priority;

	res = proc_threadCreate(NULL, thread_check_args, NULL, p, 1024, NULL, 0, (void*)arg);
	TEST_ASSERT_EQUAL_INT(EOK, res);

	proc_reap();
}

static void thread_check_prio(void *args)
{
	TEST_ASSERT_EQUAL_UINT(create_prio, proc_current()->priority);
	proc_threadEnd();
}

TEST(test_thread_spawn, priority)
{
	int res;

	create_prio = proc_current()->priority;

	res = proc_threadCreate(NULL, thread_check_prio, NULL, create_prio, 1024, NULL, 0, NULL);
	TEST_ASSERT_EQUAL_INT(EOK, res);

	proc_reap();
}

static void thread_dummy(void *args)
{
	FAIL("thread should not execute");
}

TEST(test_thread_spawn, incorrect_priority)
{
	int res;

	res = proc_threadCreate(NULL, thread_dummy, NULL, 8, 1024, NULL, 0, NULL);
	TEST_ASSERT_EQUAL_INT(-EINVAL, res);
}

TEST_GROUP_RUNNER(test_thread_spawn)
{
	RUN_TEST_CASE(test_thread_spawn, thread_end);
	RUN_TEST_CASE(test_thread_spawn, passing_arguments);
	RUN_TEST_CASE(test_thread_spawn, priority);
	RUN_TEST_CASE(test_thread_spawn, incorrect_priority);
}
