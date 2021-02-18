#include HAL
#include "include/errno.h"
#include "proc/threads.h"
#include "test/test.h"

#define THREADS_MAX_PRIO 8

volatile int actual_prio;
volatile int lock;
volatile int cpu_lock;

TEST_GROUP(test_sched);

TEST_SETUP(test_sched)
{
}

TEST_TEAR_DOWN(test_sched)
{
}

static void thread_prio(void *args)
{
	/* Here is no need for lock because a task with the highest priorty is chosen. */
	unsigned int prio = (unsigned int)args;
	TEST_ASSERT_EQUAL_INT(++actual_prio, prio);
	proc_threadEnd();
}

TEST(test_sched, priority)
{
	unsigned int stacksz = 1024;
	actual_prio = -1;

	for (unsigned int p = 0; p < THREADS_MAX_PRIO; p++) {
		int res = proc_threadCreate(NULL, thread_prio, NULL, p, stacksz, NULL, 0, (void *)p);
		TEST_ASSERT_EQUAL_INT(EOK, res);
	}

	for (unsigned int p = 0; p < THREADS_MAX_PRIO; p++) {
		proc_reap();
	}
}


static void thread_unlock(void *args)
{
	lock = 0;
	proc_threadEnd();
}

TEST(test_sched, basic_preempt)
{
	int res;
	unsigned int p = proc_current()->priority;
	lock = 1;

	res = proc_threadCreate(NULL, thread_unlock, NULL, p, 1024, NULL, 0, NULL);
	TEST_ASSERT_EQUAL_INT(EOK, res);

	/* We should be preempted, lock will be cleared by the thread_unlock task */
	while (lock) { }
	TEST_ASSERT_EQUAL_INT(0, lock);

	proc_reap();
}

static void thread_busy(void *args)
{
	while (cpu_lock);
	proc_threadEnd();
}

TEST_GROUP_RUNNER(test_sched)
{
	/* Keep other CPU busy with highest priorty task */
	cpu_lock = 1;
	for (int i = 0; i < hal_cpuGetCount() - 1; i++) {
		int res = proc_threadCreate(NULL, thread_busy, NULL, 0, 1024, NULL, 0, NULL);
		TEST_ASSERT_EQUAL_INT(EOK, res);
	}

	RUN_TEST_CASE(test_sched, priority);
	RUN_TEST_CASE(test_sched, basic_preempt);

	cpu_lock = 0;
	for (int i = 0; i < hal_cpuGetCount() - 1; i++) {
		proc_reap();
	}
}
