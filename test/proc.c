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

#include "hal/hal.h"
#include "proc/proc.h"
#include "syspage.h"


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

	/* MISRA Rule 17.7: Unused return value, (void) added in lines 41, 45, 62*/
	(void)lib_printf("test: [proc.threads] Starting indicating thread\n");
	hal_consolePrint(ATTR_USER, "\033[?25l");

	for (;;) {
		(void)lib_printf("\rtest: [proc.threads] %c %c %c %c %c %c %c  %02d %02d %02d %02d %02d %02d %02d",
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

		(void)proc_threadSleep(5000);
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
		/* MISRA Rule 17.7: Unused return value, (void) added in lines 87, 89*/
		(void)proc_threadWakeup(&test_proc_common.queue);
		hal_spinlockClear(&test_proc_common.spinlock, &sc);
		(void)proc_threadSleep(10000);
	}
}


/*
 * Thread test without conditional waiting
 */


static void test_proc_rotthr1(void *arg)
{
	/* MISRA Rule 11.6: Casted arg to a pointer type */
	unsigned long i = (unsigned long)(unsigned long *)arg;

	for (;;) {
		test_proc_common.rotations[i]++;
		/* MISRA Rule 17.7: Unused return value, (void) added */
		(void)proc_threadSleep(10000 * (i));
	}

	return;
}


void test_proc_threads1(void)
{
	unsigned int i, stacksz = 1384;
	/* MISRA Rule 15.6: Added {}*/
	for (i = 0; i < 8U; i++) {
		test_proc_common.rotations[i] = 0;
	}

	/* MISRA Rule 17.7: Unused return value, (void) added in lines 123-132*/
	(void)proc_threadCreate(NULL, test_proc_indthr, NULL, 0, stacksz, NULL, 0, NULL);
	(void)proc_threadCreate(NULL, test_proc_rotthr1, NULL, 1, stacksz, NULL, 0, (void *)(int *)1);
	(void)proc_threadCreate(NULL, test_proc_rotthr1, NULL, 2, stacksz, NULL, 0, (void *)(int *)2);
	(void)proc_threadCreate(NULL, test_proc_rotthr1, NULL, 3, stacksz, NULL, 0, (void *)(int *)3);
	(void)proc_threadCreate(NULL, test_proc_rotthr1, NULL, 4, stacksz, NULL, 0, (void *)(int *)4);
	(void)proc_threadCreate(NULL, test_proc_rotthr1, NULL, 5, stacksz, NULL, 0, (void *)(int *)5);
	(void)proc_threadCreate(NULL, test_proc_rotthr1, NULL, 6, stacksz, NULL, 0, (void *)(int *)6);
	(void)proc_threadCreate(NULL, test_proc_rotthr1, NULL, 7, stacksz, NULL, 0, (void *)(int *)7);

	(void)proc_threadCreate(NULL, test_proc_busythr, NULL, 4, 1024, NULL, 0, NULL);
}


/*
 * Thread test with conditional waiting
 */


static void test_proc_rotthr2(void *arg)
{
	/* MISRA Rule 11.6: Casted arg to a pointer type */
	unsigned long i = (unsigned long)(unsigned long *)arg;
	time_t otm = test_proc_common.tm;
	spinlock_ctx_t sc;

	for (;;) {
		test_proc_common.rotations[i]++;

		hal_spinlockSet(&test_proc_common.spinlock, &sc);
		for (;;) {
			/* MISRA Rule 17.7: Unused return value, (void) added */
			(void)proc_threadWait(&test_proc_common.queue, &test_proc_common.spinlock, 0, &sc);
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
	/* MISRA Rule 15.6: Added {} */
	for (i = 0; i < 8; i++) {
		test_proc_common.rotations[i] = 0;
	}

	test_proc_common.tm = 0;
	test_proc_common.queue = NULL;
	hal_spinlockCreate(&test_proc_common.spinlock, "test_proc_common.spinlock");

	/* MISRA Rule 17.7: Unused return value, (void) added in lines 179-185*/
	(void)proc_threadCreate(NULL, test_proc_indthr, NULL, 0, 1024, NULL, 0, NULL);
	(void)proc_threadCreate(NULL, test_proc_timethr, NULL, 0, 1024, NULL, 0, NULL);

	(void)proc_threadCreate(NULL, test_proc_rotthr2, NULL, 1, 1024, NULL, 0, (void *)(int *)1);
	(void)proc_threadCreate(NULL, test_proc_rotthr2, NULL, 2, 1024, NULL, 0, (void *)(int *)2);
	(void)proc_threadCreate(NULL, test_proc_rotthr2, NULL, 3, 1024, NULL, 0, (void *)(int *)3);
	(void)proc_threadCreate(NULL, test_proc_rotthr2, NULL, 4, 1024, NULL, 0, (void *)(int *)4);
}


/* Test process termination given terminating programs in syspage */
static void test_proc_initthr(void *arg)
{
	syspage_prog_t *prog;
	char *argv[] = { "syspage", "arg1", "arg2", "arg3", NULL };

	/* Enable locking and multithreading related mechanisms */
	_hal_start();

	/* MISRA Rule 17.7: Unused return value, (void) added in lines 199, 200, 205, 208*/
	(void)lib_printf("main: Starting syspage programs (%d) and init\n", syspage_progSize());
	(void)lib_printf("init: %p\n", proc_current());

	for (;;) {
		if ((prog = syspage_progList()) != NULL) {
			do {
				(void)proc_syspageSpawn(prog, NULL, NULL, "", argv);
			} while ((prog = prog->next) != syspage_progList());
		}
		(void)proc_threadSleep(120000);
	}
}

void test_proc_exit(void)
{
	/* MISRA Rule 17.7: Unused return value, (void) added in lines 215, 218*/
	(void)proc_start(test_proc_initthr, NULL, (const char *)"init");

	hal_cpuEnableInterrupts();
	(void)hal_cpuReschedule(NULL, NULL);
}
