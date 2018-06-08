/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for proc subsystem
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../proc/proc.h"


struct {
	volatile unsigned int rotations[8];
	volatile time_t tm;
	spinlock_t spinlock;
	thread_t *queue;
	unsigned int port;
} test_proc_common;


/*
 * Common threads
 */


static void test_proc_indthr(void *arg)
{
	char *indicator = "o|/-\\|/-\\";

	lib_printf("test: [proc.threads] Starting indicating thread\n");
	hal_consolePrint(ATTR_USER, "\033[?25l");

	for (;;) {
		lib_printf("\rtest: [proc.threads] %c %c %c %c %c %c %c  %02d %02d %02d %02d %02d %02d %02d",
			indicator[test_proc_common.rotations[1] % 8],
			indicator[test_proc_common.rotations[2] % 8],
			indicator[test_proc_common.rotations[3] % 8],
			indicator[test_proc_common.rotations[4] % 8],
			indicator[test_proc_common.rotations[5] % 8],
			indicator[test_proc_common.rotations[6] % 8],
			indicator[test_proc_common.rotations[7] % 8],

			test_proc_common.rotations[1] % 100,
			test_proc_common.rotations[2] % 100,
			test_proc_common.rotations[3] % 100,
			test_proc_common.rotations[4] % 100,
			test_proc_common.rotations[5] % 100,
			test_proc_common.rotations[6] % 100,
			test_proc_common.rotations[7] % 100);

		proc_threadSleep(5000);
	}

	return;
}


static void test_proc_busythr(void *arg)
{
	for (;;)
		hal_cpuHalt();

	return;
}


static void test_proc_timethr(void *arg)
{
	for (;;) {
		hal_spinlockSet(&test_proc_common.spinlock);
		test_proc_common.tm++;
		proc_threadWakeup(&test_proc_common.queue);
		hal_spinlockClear(&test_proc_common.spinlock);
		proc_threadSleep(100000);
	}
}


/*
 * Thread test without conditional waiting
 */


static void test_proc_rotthr1(void *arg)
{
	unsigned int i = (unsigned long)arg;

	for (;;) {
		test_proc_common.rotations[i]++;
		proc_threadSleep(10000 * i);
	}

	return;
}


void test_proc_threads1(void)
{
	unsigned int i, stacksz = 1384;

	for (i = 0; i < 8; i++)
		test_proc_common.rotations[i] = 0;

	proc_threadCreate(NULL, test_proc_indthr, NULL, 0, stacksz, NULL, 0, NULL);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 1, stacksz, NULL, 0, (void *)1);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 2, stacksz, NULL, 0, (void *)2);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 3, stacksz, NULL, 0, (void *)3);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 4, stacksz, NULL, 0, (void *)4);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 5, stacksz, NULL, 0, (void *)5);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 6, stacksz, NULL, 0, (void *)6);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 7, stacksz, NULL, 0, (void *)7);

	proc_threadCreate(NULL, test_proc_busythr, NULL, 4, 1024, NULL, 0, NULL);
}


/*
 * Thread test with conditional waiting
 */


static void test_proc_rotthr2(void *arg)
{
	unsigned int i = (unsigned long)arg;
	time_t otm = test_proc_common.tm;

	for (;;) {
		test_proc_common.rotations[i]++;

		hal_spinlockSet(&test_proc_common.spinlock);
		for (;;) {
			proc_threadWait(&test_proc_common.queue, &test_proc_common.spinlock, 0);
			if (test_proc_common.tm > otm) {
				otm = test_proc_common.tm;
				break;
			}
		}
		hal_spinlockClear(&test_proc_common.spinlock);
	}
	return;
}


void test_proc_threads2(void)
{
	unsigned int i;

	for (i = 0; i < 8; i++)
		test_proc_common.rotations[i] = 0;

	test_proc_common.tm = 0;
	test_proc_common.queue = NULL;
	hal_spinlockCreate(&test_proc_common.spinlock, "test_proc_common.spinlock");

	proc_threadCreate(NULL, test_proc_indthr, NULL, 0, 1024, NULL, 0, NULL);
	proc_threadCreate(NULL, test_proc_timethr, NULL, 0, 1024, NULL, 0, NULL);

	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 1, 1024, NULL, 0, (void *)1);
	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 2, 1024, NULL, 0, (void *)2);
	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 3, 1024, NULL, 0, (void *)3);
	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 4, 1024, NULL, 0, (void *)4);
}


/* Test process termination given terminating programs in syspage */
static void test_proc_initthr(void *arg)
{
	unsigned int i;
	syspage_program_t *prog;

	/* Enable locking and multithreading related mechanisms */
	_hal_start();

	lib_printf("main: Starting syspage programs (%d) and init\n", syspage->progssz);
	lib_printf("init: %p\n", proc_current());

	for (;;) {
		for (prog = syspage->progs, i = 0; i < syspage->progssz; i++, prog++) {
			if (!proc_vfork())
				proc_execle(prog, "", "syspage", "arg1", "arg2", "arg3", NULL, NULL);
		}

		proc_threadSleep(120000);
	}
}

void test_proc_exit(void)
{
	proc_start(test_proc_initthr, NULL, (const char *)"init");

	hal_cpuEnableInterrupts();
	hal_cpuReschedule(NULL);
}
