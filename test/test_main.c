#include "test.h"
#include "proc/threads.h"

void runner(void)
{
}

void test_main(void* args)
{
	int verbose = 1;

	UnityMain((const char *)"kernel", runner, verbose);

	proc_threadEnd();
}
