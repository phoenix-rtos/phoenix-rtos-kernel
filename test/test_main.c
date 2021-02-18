#include "test.h"
#include "proc/threads.h"

DECLARE_TEST_GROUP(test_list);
DECLARE_TEST_GROUP(test_sched);
DECLARE_TEST_GROUP(test_bsearch);

void runner(void)
{
	RUN_TEST_GROUP(test_list);
	RUN_TEST_GROUP(test_sched);
	RUN_TEST_GROUP(test_bsearch);
}

void test_main(void* args)
{
	int verbose = 1;

	UnityMain((const char *)"kernel", runner, verbose);

	proc_threadEnd();
}
