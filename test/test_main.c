#include "test.h"
#include "proc/threads.h"

DECLARE_TEST_GROUP(test_list);

void runner(void)
{
	RUN_TEST_GROUP(test_list);
}

void test_main(void* args)
{
	int verbose = 1;

	UnityMain((const char *)"kernel", runner, verbose);

	proc_threadEnd();
}
