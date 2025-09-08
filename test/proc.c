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
#include "lib/lib.h"
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

	lib_printf("test: [proc.threads] Starting indicating thread\n");
	hal_consolePrint(ATTR_USER, "\033[?25l");

	for (;;) {
		lib_printf("\rtest: [proc.threads] %c %c %c %c %c %c %c  %02d %02d %02d %02d %02d %02d %02d",
				indicator[test_proc_common.rotations[1] % 8U],
				indicator[test_proc_common.rotations[2] % 8U],
				indicator[test_proc_common.rotations[3] % 8U],
				indicator[test_proc_common.rotations[4] % 8U],
				indicator[test_proc_common.rotations[5] % 8U],
				indicator[test_proc_common.rotations[6] % 8U],
				indicator[test_proc_common.rotations[7] % 8U],

				test_proc_common.rotations[1] % 100U,
				test_proc_common.rotations[2] % 100U,
				test_proc_common.rotations[3] % 100U,
				test_proc_common.rotations[4] % 100U,
				test_proc_common.rotations[5] % 100U,
				test_proc_common.rotations[6] % 100U,
				test_proc_common.rotations[7] % 100U);

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
	for (i = 0; i < 8U; i++) {
		test_proc_common.rotations[i] = 0;
	}

	proc_threadCreate(NULL, test_proc_indthr, NULL, 0, stacksz, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 1, stacksz, NULL, 0, 0, (void *)(int *)1);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 2, stacksz, NULL, 0, 0, (void *)(int *)2);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 3, stacksz, NULL, 0, 0, (void *)(int *)3);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 4, stacksz, NULL, 0, 0, (void *)(int *)4);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 5, stacksz, NULL, 0, 0, (void *)(int *)5);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 6, stacksz, NULL, 0, 0, (void *)(int *)6);
	proc_threadCreate(NULL, test_proc_rotthr1, NULL, 7, stacksz, NULL, 0, 0, (void *)(int *)7);

	proc_threadCreate(NULL, test_proc_busythr, NULL, 4, 1024, NULL, 0, 0, NULL);
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
	for (i = 0; i < 8U; i++) {
		test_proc_common.rotations[i] = 0;
	}

	test_proc_common.tm = 0;
	test_proc_common.queue = NULL;
	hal_spinlockCreate(&test_proc_common.spinlock, "test_proc_common.spinlock");

	proc_threadCreate(NULL, test_proc_indthr, NULL, 0, 1024, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_proc_timethr, NULL, 0, 1024, NULL, 0, 0, NULL);

	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 1, 1024, NULL, 0, 0, (void *)(int *)1);
	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 2, 1024, NULL, 0, 0, (void *)(int *)2);
	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 3, 1024, NULL, 0, 0, (void *)(int *)3);
	proc_threadCreate(NULL, test_proc_rotthr2, NULL, 4, 1024, NULL, 0, 0, (void *)(int *)4);
}


/*
 * Priority inheritance tests
 *
 * Isolated repros for the PROTO_INHERIT / BWI SC-donation lock code:
 * simple single-waiter boost, multi-waiter boost + hand-off, a chained
 * (nested-lock) boost that must transitively propagate through an
 * intermediate owner, and a stress test mirroring proc_threadsIter()'s
 * "hold an outer lock, take/release inner locks per iteration" pattern
 * under concurrent contention on both.
 */


struct {
	lock_t lockA;
	lock_t lockB;

	thread_t *tLow;
	thread_t *tHigh;

	thread_t *tOwner;

	thread_t *tA;
	thread_t *tB;
	thread_t *tC;
} test_pi_common;


#define TEST_PI_STRESS_INNER_LOCKS 4
#define TEST_PI_STRESS_ITERS       200

struct {
	lock_t outer;
	lock_t inner[TEST_PI_STRESS_INNER_LOCKS];
} test_pi_stress;


/*
 * Scenario 1: simple single-waiter boost.
 */

static void test_pi_lowThr(void *arg)
{
	test_pi_common.tLow = proc_current();

	(void)proc_lockSet(&test_pi_common.lockA);
	lib_printf("test: [pi.simple] low-prio thread acquired lockA\n");
	proc_threadSleep(300000);
	(void)proc_lockClear(&test_pi_common.lockA);
	lib_printf("test: [pi.simple] low-prio thread released lockA\n");

	proc_threadEnd();
}


static void test_pi_highThr(void *arg)
{
	proc_threadSleep(50000);
	test_pi_common.tHigh = proc_current();

	lib_printf("test: [pi.simple] high-prio thread blocking on lockA\n");
	(void)proc_lockSet(&test_pi_common.lockA);
	lib_printf("test: [pi.simple] high-prio thread acquired lockA\n");
	(void)proc_lockClear(&test_pi_common.lockA);

	proc_threadEnd();
}


static void test_pi_simpleChecker(void *arg)
{
	int prio;

	proc_threadSleep(150000);
	prio = proc_threadPriority(test_pi_common.tLow, -1);
	lib_printf("test: [pi.simple] low-prio thread's effective priority while blocking high-prio waiter: %d\n", prio);
	LIB_ASSERT(prio == 2, "PI FAILED: low-prio thread not boosted to waiter's priority (got %d, expected 2)", prio);

	proc_threadSleep(250000);
	prio = proc_threadPriority(test_pi_common.tLow, -1);
	lib_printf("test: [pi.simple] low-prio thread's priority after release: %d\n", prio);
	LIB_ASSERT(prio == 10, "PI FAILED: low-prio thread priority not restored (got %d, expected 10)", prio);

	lib_printf("test: [pi.simple] PASSED\n");

	proc_threadEnd();
}


static void test_proc_priorityInheritSimple(void)
{
	(void)proc_lockInit(&test_pi_common.lockA, &proc_lockAttrDefault, "test.pi.lockA");

	proc_threadCreate(NULL, test_pi_lowThr, NULL, 10, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_pi_highThr, NULL, 2, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_pi_simpleChecker, NULL, 1, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 2: multiple waiters queued on the same lock - the owner must be
 * boosted to the best of them, and the boost must correctly migrate to
 * whoever the lock is handed off to (bug #2 in the BWI rework).
 */


static void test_pi_mwOwner(void *arg)
{
	test_pi_common.tOwner = proc_current();

	(void)proc_lockSet(&test_pi_common.lockA);
	lib_printf("test: [pi.multiwaiter] owner acquired lockA\n");
	proc_threadSleep(300000);
	(void)proc_lockClear(&test_pi_common.lockA);
	lib_printf("test: [pi.multiwaiter] owner released lockA\n");

	proc_threadEnd();
}


static void test_pi_mwWaiter(void *arg)
{
	unsigned long id = (unsigned long)arg;

	proc_threadSleep(30000 + 10000UL * id);
	lib_printf("test: [pi.multiwaiter] waiter %lu blocking on lockA\n", id);
	(void)proc_lockSet(&test_pi_common.lockA);
	lib_printf("test: [pi.multiwaiter] waiter %lu acquired lockA\n", id);
	proc_threadSleep(20000);
	(void)proc_lockClear(&test_pi_common.lockA);
	lib_printf("test: [pi.multiwaiter] waiter %lu released lockA\n", id);

	proc_threadEnd();
}


static void test_pi_mwChecker(void *arg)
{
	int prio;

	proc_threadSleep(150000);
	prio = proc_threadPriority(test_pi_common.tOwner, -1);
	lib_printf("test: [pi.multiwaiter] owner's effective priority with 3 waiters queued (prios 8,4,10): %d\n", prio);
	LIB_ASSERT(prio == 4, "PI FAILED: owner not boosted to best of the queued waiters (got %d, expected 4)", prio);

	proc_threadSleep(400000);
	prio = proc_threadPriority(test_pi_common.tOwner, -1);
	lib_printf("test: [pi.multiwaiter] owner's priority once all waiters are done: %d\n", prio);
	LIB_ASSERT(prio == 12, "PI FAILED: owner priority not restored after all waiters released (got %d, expected 12)", prio);

	lib_printf("test: [pi.multiwaiter] PASSED\n");

	proc_threadEnd();
}


static void test_proc_priorityInheritMultiwaiter(void)
{
	(void)proc_lockInit(&test_pi_common.lockA, &proc_lockAttrDefault, "test.pi.lockA");

	proc_threadCreate(NULL, test_pi_mwOwner, NULL, 12, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_pi_mwWaiter, NULL, 8, 2048, NULL, 0, 0, (void *)(unsigned long)0);
	proc_threadCreate(NULL, test_pi_mwWaiter, NULL, 4, 2048, NULL, 0, 0, (void *)(unsigned long)1);
	proc_threadCreate(NULL, test_pi_mwWaiter, NULL, 10, 2048, NULL, 0, 0, (void *)(unsigned long)2);
	proc_threadCreate(NULL, test_pi_mwChecker, NULL, 1, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 3: chained/nested lock boost. A holds lockA and is itself blocked
 * waiting on lockB (held by B) - exactly the proc_threadsIter() pattern
 * (hold one lock, wait on another). C then blocks on lockA, held by A. The
 * boost must transitively propagate all the way to B, the ultimate blocker,
 * not just stop at A (bug in the original single-hop BWI donation).
 */


static void test_pi_chainB(void *arg)
{
	test_pi_common.tB = proc_current();

	(void)proc_lockSet(&test_pi_common.lockB);
	lib_printf("test: [pi.chain] B acquired lockB\n");
	proc_threadSleep(400000);
	(void)proc_lockClear(&test_pi_common.lockB);
	lib_printf("test: [pi.chain] B released lockB\n");

	proc_threadEnd();
}


static void test_pi_chainA(void *arg)
{
	proc_threadSleep(50000);
	test_pi_common.tA = proc_current();

	(void)proc_lockSet(&test_pi_common.lockA);
	lib_printf("test: [pi.chain] A acquired lockA\n");
	proc_threadSleep(50000);

	lib_printf("test: [pi.chain] A blocking on lockB (held by B)\n");
	(void)proc_lockSet(&test_pi_common.lockB);
	lib_printf("test: [pi.chain] A acquired lockB\n");
	proc_threadSleep(20000);
	(void)proc_lockClear(&test_pi_common.lockB);
	(void)proc_lockClear(&test_pi_common.lockA);
	lib_printf("test: [pi.chain] A released both locks\n");

	proc_threadEnd();
}


static void test_pi_chainC(void *arg)
{
	proc_threadSleep(150000);
	test_pi_common.tC = proc_current();

	lib_printf("test: [pi.chain] C blocking on lockA (held by A)\n");
	(void)proc_lockSet(&test_pi_common.lockA);
	lib_printf("test: [pi.chain] C acquired lockA\n");
	(void)proc_lockClear(&test_pi_common.lockA);
	lib_printf("test: [pi.chain] C released lockA\n");

	proc_threadEnd();
}


static void test_pi_chainChecker(void *arg)
{
	int prioA, prioB, prioC;

	proc_threadSleep(250000);
	prioA = proc_threadPriority(test_pi_common.tA, -1);
	prioB = proc_threadPriority(test_pi_common.tB, -1);
	lib_printf("test: [pi.chain] mid-chain (A owns lockA + waits on lockB, C waits on lockA): A=%d B=%d\n", prioA, prioB);
	LIB_ASSERT(prioB == 2, "CHAIN PI FAILED: ultimate blocker B not boosted transitively through A (got %d, expected 2)", prioB);

	proc_threadSleep(400000);
	prioA = proc_threadPriority(test_pi_common.tA, -1);
	prioB = proc_threadPriority(test_pi_common.tB, -1);
	prioC = proc_threadPriority(test_pi_common.tC, -1);
	lib_printf("test: [pi.chain] after everyone finished: A=%d B=%d C=%d\n", prioA, prioB, prioC);
	LIB_ASSERT(prioA == 10, "CHAIN PI FAILED: A's priority not restored (got %d, expected 10)", prioA);
	LIB_ASSERT(prioB == 12, "CHAIN PI FAILED: B's priority not restored (got %d, expected 12)", prioB);
	LIB_ASSERT(prioC == 2, "CHAIN PI FAILED: C's priority not restored (got %d, expected 2)", prioC);

	lib_printf("test: [pi.chain] PASSED\n");

	proc_threadEnd();
}


static void test_proc_priorityInheritChain(void)
{
	(void)proc_lockInit(&test_pi_common.lockA, &proc_lockAttrDefault, "test.pi.lockA");
	(void)proc_lockInit(&test_pi_common.lockB, &proc_lockAttrDefault, "test.pi.lockB");

	proc_threadCreate(NULL, test_pi_chainB, NULL, 12, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_pi_chainA, NULL, 10, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_pi_chainC, NULL, 2, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_pi_chainChecker, NULL, 1, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 4: stress test mirroring proc_threadsIter()'s exact lock pattern -
 * one thread repeatedly holds an "outer" lock while taking/releasing several
 * "inner" locks per iteration, while other threads at varying priorities
 * concurrently contend for both the outer lock and the inner locks
 * directly. This is the pattern that originally produced the
 * "lock is not on the list" crash in threads.common/kmalloc.common.
 */


static void test_pi_stressIterator(void *arg)
{
	unsigned int i, iter;

	for (iter = 0; iter < TEST_PI_STRESS_ITERS; iter++) {
		(void)proc_lockSet(&test_pi_stress.outer);
		for (i = 0; i < TEST_PI_STRESS_INNER_LOCKS; i++) {
			(void)proc_lockSet(&test_pi_stress.inner[i]);
			proc_threadSleep(200);
			(void)proc_lockClear(&test_pi_stress.inner[i]);
		}
		(void)proc_lockClear(&test_pi_stress.outer);
		proc_threadSleep(500);
	}

	lib_printf("test: [pi.stress] iterator done\n");

	proc_threadEnd();
}


static void test_pi_stressContender(void *arg)
{
	unsigned long id = (unsigned long)arg;
	unsigned int iter;

	for (iter = 0; iter < TEST_PI_STRESS_ITERS; iter++) {
		if ((iter & 1U) != 0U) {
			(void)proc_lockSet(&test_pi_stress.outer);
			proc_threadSleep(300);
			(void)proc_lockClear(&test_pi_stress.outer);
		}
		else {
			(void)proc_lockSet(&test_pi_stress.inner[id % TEST_PI_STRESS_INNER_LOCKS]);
			proc_threadSleep(300);
			(void)proc_lockClear(&test_pi_stress.inner[id % TEST_PI_STRESS_INNER_LOCKS]);
		}
		proc_threadSleep(100 + 50UL * id);
	}

	lib_printf("test: [pi.stress] contender %lu done\n", id);

	proc_threadEnd();
}


static void test_proc_priorityInheritStress(void)
{
	unsigned int i;

	(void)proc_lockInit(&test_pi_stress.outer, &proc_lockAttrDefault, "test.pi.stress.outer");
	for (i = 0; i < TEST_PI_STRESS_INNER_LOCKS; i++) {
		(void)proc_lockInit(&test_pi_stress.inner[i], &proc_lockAttrDefault, "test.pi.stress.inner");
	}

	proc_threadCreate(NULL, test_pi_stressIterator, NULL, 8, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_pi_stressContender, NULL, 3, 2048, NULL, 0, 0, (void *)(unsigned long)0);
	proc_threadCreate(NULL, test_pi_stressContender, NULL, 12, 2048, NULL, 0, 0, (void *)(unsigned long)1);
	proc_threadCreate(NULL, test_pi_stressContender, NULL, 6, 2048, NULL, 0, 0, (void *)(unsigned long)2);
	proc_threadCreate(NULL, test_pi_stressContender, NULL, 14, 2048, NULL, 0, 0, (void *)(unsigned long)3);
}


/* Runs all priority-inheritance scenarios in sequence, isolated from the rest
 * of the boot process (no syspage programs, no userspace). */
static void test_proc_priorityInheritInitthr(void *arg)
{
	/* Enable locking and multithreading related mechanisms */
	_hal_start();

	lib_printf("test: [pi] starting priority inheritance test suite\n");

	test_proc_priorityInheritSimple();
	proc_threadSleep(500000);

	test_proc_priorityInheritMultiwaiter();
	proc_threadSleep(600000);

	test_proc_priorityInheritChain();
	proc_threadSleep(700000);

	lib_printf("test: [pi] starting stress scenario (mirrors proc_threadsIter's nested-lock pattern)\n");
	test_proc_priorityInheritStress();

	for (;;) {
		proc_reap();
	}
}


void test_proc_priorityInherit(void)
{
	proc_start(test_proc_priorityInheritInitthr, NULL, (const char *)"pitest");

	hal_cpuEnableInterrupts();
	hal_cpuReschedule(NULL, NULL);
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
