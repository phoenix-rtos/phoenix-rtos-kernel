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
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* parasoft-begin-suppress ALL "tests don't need to comply with MISRA" */

#include "hal/hal.h"
#include "proc/proc.h"
#include "syspage.h"


struct {
	volatile unsigned int rotations[NPRIOS];
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
	const char *indicator = "o|/-\\|/-\\";
	const int k = 8;
	int i;

	lib_printf("test: [proc.threads] Starting indicating thread\n");
	hal_consolePrint(ATTR_USER, "\033[?25l");

	for (;;) {
		for (i = 0; i < k; i++) {
			lib_printf("test: [proc.threads] %02d %c %c %c %c %c %c %c %c  %02d %02d %02d %02d %02d %02d %02d %02d\n",
					i * k,
					indicator[test_proc_common.rotations[i * k + 0] % 8U],
					indicator[test_proc_common.rotations[i * k + 1] % 8U],
					indicator[test_proc_common.rotations[i * k + 2] % 8U],
					indicator[test_proc_common.rotations[i * k + 3] % 8U],
					indicator[test_proc_common.rotations[i * k + 4] % 8U],
					indicator[test_proc_common.rotations[i * k + 5] % 8U],
					indicator[test_proc_common.rotations[i * k + 6] % 8U],
					indicator[test_proc_common.rotations[i * k + 7] % 8U],

					test_proc_common.rotations[i * k + 0] % 100U,
					test_proc_common.rotations[i * k + 1] % 100U,
					test_proc_common.rotations[i * k + 2] % 100U,
					test_proc_common.rotations[i * k + 3] % 100U,
					test_proc_common.rotations[i * k + 4] % 100U,
					test_proc_common.rotations[i * k + 5] % 100U,
					test_proc_common.rotations[i * k + 6] % 100U,
					test_proc_common.rotations[i * k + 7] % 100U);
		}

		lib_printf("\033[8A\r");

		proc_threadSleep(5000);
	}

	return;
}


static void test_proc_busythr(void *arg)
{
	for (;;) {
		hal_cpuHalt();
	}

	return;
}


static void test_proc_timethr(void *arg)
{
	spinlock_ctx_t sc;

	for (;;) {
		hal_spinlockSet(&test_proc_common.spinlock, &sc);
		test_proc_common.tm++;
		proc_threadWakeup(&test_proc_common.queue);
		hal_spinlockClear(&test_proc_common.spinlock, &sc);
		proc_threadSleep(10000);
	}
}


/*
 * Thread test without conditional waiting
 */


static void test_proc_rotthr1(void *arg)
{
	unsigned long i = (unsigned long)arg;

	for (;;) {
		test_proc_common.rotations[i]++;
		proc_threadSleep(10000ULL * (i));
	}

	return;
}


void test_proc_threads1(void)
{
	unsigned int i, stacksz = 1384;
	for (i = 0; i < NPRIOS; i++) {
		test_proc_common.rotations[i] = 0;
	}

	proc_threadCreate(NULL, test_proc_indthr, NULL, MIN_PRIO, stacksz, NULL, 0, 0, NULL);

	for (i = 1; i < NPRIOS; i++) {
		proc_threadCreate(NULL, test_proc_rotthr1, NULL, MIN_PRIO + i, stacksz, NULL, 0, 0, (void *)(ptr_t)i);
	}

	proc_threadCreate(NULL, test_proc_busythr, NULL, (MAX_PRIO - MIN_PRIO) / 2, 1024, NULL, 0, 0, NULL);
}


/*
 * Thread test with conditional waiting
 */


static void test_proc_rotthr2(void *arg)
{
	unsigned long i = (unsigned long)arg;
	time_t otm = test_proc_common.tm;
	spinlock_ctx_t sc;

	for (;;) {
		test_proc_common.rotations[i]++;

		hal_spinlockSet(&test_proc_common.spinlock, &sc);
		for (;;) {
			proc_threadWait(&test_proc_common.queue, &test_proc_common.spinlock, 0, &sc);
			if (test_proc_common.tm > otm) {
				otm = test_proc_common.tm;
				break;
			}
		}
		hal_spinlockClear(&test_proc_common.spinlock, &sc);
	}
	return;
}


void test_proc_threads2(void)
{
	unsigned int i;
	for (i = 0; i < NPRIOS; i++) {
		test_proc_common.rotations[i] = 0;
	}

	test_proc_common.tm = 0;
	test_proc_common.queue = NULL;
	hal_spinlockCreate(&test_proc_common.spinlock, "test_proc_common.spinlock");

	proc_threadCreate(NULL, test_proc_indthr, NULL, MIN_PRIO, 1024, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_proc_timethr, NULL, MIN_PRIO, 1024, NULL, 0, 0, NULL);

	for (i = 8; i < NPRIOS; i += 8) {
		proc_threadCreate(NULL, test_proc_rotthr2, NULL, MIN_PRIO + i, 1024, NULL, 0, 0, (void *)(ptr_t)i);
	}
}


/* Test process termination given terminating programs in syspage */
static void test_proc_initthr(void *arg)
{
	syspage_prog_t *prog;
	char *argv[] = { "syspage", "arg1", "arg2", "arg3", NULL };

	/* Enable locking and multithreading related mechanisms */
	_hal_start();

	lib_printf("main: Starting syspage programs (%d) and init\n", syspage_progSize());
	lib_printf("init: %p\n", proc_current());

	for (;;) {
		if ((prog = syspage_progList()) != NULL) {
			do {
				proc_syspageSpawn(prog, NULL, NULL, "", argv);
			} while ((prog = prog->next) != syspage_progList());
		}
		proc_threadSleep(120000);
	}
}

void test_proc_exit(void)
{
	proc_start(test_proc_initthr, NULL, (const char *)"init");

	hal_cpuEnableInterrupts();
	hal_cpuReschedule(NULL, NULL);
}

/* parasoft-end-suppress ALL "tests don't need to comply with MISRA" */
