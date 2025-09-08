/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for messages
 *
 * Copyright 2017 Phoenix Systems
 * Author: Jakub Sejdak, Jan Sikorski, Pawel Pisarczyk
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* parasoft-begin-suppress ALL "tests don't need to comply with MISRA" */

#include "hal/hal.h"
#include "include/errno.h"
#include "proc/proc.h"
#include "proc/threads.h"
#include "proc/ports.h"
#include "proc/xfer.h"


unsigned int test_randsize(unsigned int *seed, unsigned int bufsz)
{
	unsigned int sz;

	if (lib_rand(seed) % 2) {
		sz = ((unsigned int)lib_rand(seed) % (bufsz / SIZE_PAGE)) * SIZE_PAGE;
	}
	else {
		sz = 1U + ((unsigned int)lib_rand(seed) % bufsz);
	}

	return (sz != 0U) ? sz : 1U;
}


unsigned int test_offset(unsigned int *seed, unsigned int size, unsigned int bufsz)
{
	unsigned int offs = (bufsz - size) / SIZE_PAGE;

	if (offs != 0U && lib_rand(seed) % 2 != 0) {
		offs = ((unsigned int)lib_rand(seed) % offs) * SIZE_PAGE;
	}
	else if (offs != 0U && lib_rand(seed) % 10 != 0) {
		offs = SIZE_PAGE - (size & (SIZE_PAGE - 1U));
	}
	else if (offs != 0U && lib_rand(seed) % 10 != 0) {
		offs = SIZE_PAGE - (size & (SIZE_PAGE - 1U)) / 2U;
	}
	else if (bufsz - size != 0U) {
		offs = (unsigned int)lib_rand(seed) % (bufsz - size);
	}
	else {
		offs = 0;
	}
	return offs;
}


void test_ping(void *arg)
{
	msg_t msg;
	unsigned int bufsz = 4U * SIZE_PAGE, offs[2], i, k;
	void *buf[2];
	unsigned int seed = (unsigned long)test_ping;
	unsigned int count = 0;
	unsigned int port = (unsigned long)arg;


	lib_printf("test: msg/ping: starting\n");

	buf[0] = vm_kmalloc(bufsz);
	buf[1] = vm_kmalloc(bufsz);

	if (buf[0] == NULL || buf[1] == NULL) {
		lib_printf("test_msg/ping: could not allocate buffers\n");
		return;
	}

	for (k = 0; count == 0U || k < count; ++k) {
		lib_printf("\rtest_msg/ping: % 20d OK", k);

		hal_memset(&msg, 0, sizeof(msg));

		msg.o.size = msg.i.size = test_randsize(&seed, bufsz);

		msg.i.data = buf[0] + (offs[0] = test_offset(&seed, msg.i.size, bufsz));
		msg.o.data = buf[1] + (offs[1] = test_offset(&seed, msg.o.size, bufsz));

		for (i = 0; i < msg.o.size; ++i) {
			((unsigned char *)msg.i.data)[i] = (unsigned char)lib_rand(&seed);
		}

		if (proc_send(port, &msg) < 0) {
			lib_printf("\ntest_msg/ping: send failed\n");
			return;
		}

		if (msg.o.err < 0) {
			lib_printf("\ntest_msg/ping: pong returned error\n");
			return;
		}

		if (msg.i.size != msg.o.size) {
			lib_printf("\ntest_msg/ping: sizes mismatch\n");
			return;
		}


		/*if (memcmp(msg.o.data, msg.i.data, msg.i.size)) {
			lib_printf("\ntest_msg/ping: data mismatch\n");

			for (i = 0; i < msg.i.size; ++i) {
				lib_printf("%02x", ((unsigned char *)msg.i.data)[i]);
			}

			printf("\n");

			for (i = 0; i < msg.o.size; ++i) {
				lib_printf("%02x", ((unsigned char *)msg.o.data)[i]);
			}

			lib_printf("\n");

			return 1;
		}*/
	}

	lib_printf("\n");

	return;
}


void test_pong(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	unsigned int port = (unsigned long)arg;

	lib_printf("test_msg/pong: starting\n");

	for (;;) {

		if (proc_recv(port, &msg, &rid) < 0) {
			lib_printf("test_msg/pong: receive failed\n");
			msg.o.err = 1;
		}

		if (msg.i.size != msg.o.size) {
			lib_printf("test_msg/pong: i/o buffers are of different sizes: 0x%zx and 0x%zx\n", msg.i.size, msg.o.size);
			msg.o.err = 1;
		}
		else {
			hal_memcpy(msg.o.data, msg.i.data, msg.i.size);
		}

		(void)proc_respond(port, &msg, rid);
	}

	return;
}


void test_msg(void)
{
	unsigned int port;

	if (proc_portCreate(&port) != EOK) {
		lib_printf("Failed to create port\n");
		threads_halt();
	}

	proc_threadCreate(NULL, test_pong, NULL, 4, 1024, NULL, 0, 0, (void *)(long)port);
	proc_threadCreate(NULL, test_ping, NULL, 4, 1024, NULL, 0, 0, (void *)(long)port);
}


/*
 * Port hand-off and bandwidth-inheritance tests.
 *
 * These exercise the port queue itself: the priority-ordered hand-off of a
 * receiver that comes free to the best sender waiting for it, and the SC a
 * sender donates to a busy receiver for as long as it waits (see bwi-docs.md).
 */


#define TEST_PORT_SERVED_MAX     8
#define TEST_PORT_STRESS_ITERS   150
#define TEST_PORT_STRESS_PULSES  50
#define TEST_PORT_TEARDOWN_ITERS 200
#define TEST_PORT_CLOSE_RECVS    2
#define TEST_PORT_REFS_CLIENTS   4
#define TEST_PORT_REFS_ITERS     50

static struct {
	unsigned int order;    /* hand-off order scenario */
	unsigned int busy;     /* busy-receiver boost scenario */
	unsigned int stress;   /* stress scenario */
	unsigned int pulse;    /* pulse scenario */
	unsigned int teardown; /* teardown scenario */
	unsigned int close;    /* close scenario */
	unsigned int refs;     /* reference count scenario */

	int served[TEST_PORT_SERVED_MAX];
	unsigned int servedCnt;

	thread_t *busyServer;
	thread_t *dying;
	thread_t *client[3];

	volatile unsigned int pulses;
	volatile unsigned int clientsDone;
	volatile unsigned int teardownDone;
	volatile unsigned int closeWaiting;
	volatile unsigned int closeWoken;
	volatile unsigned int refsDone;
	volatile unsigned int refsServerDone;
	int refsBefore;

	lock_t lock;
} test_port_common;


/*
 * Scenario 1: a receiver that comes free must go to the best-priority sender
 * queued on the port, not to whoever asked first (or last).
 */


static void test_port_orderServer(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	unsigned int i;

	for (i = 0; i < TEST_PORT_SERVED_MAX; i++) {
		if (proc_recv(test_port_common.order, &msg, &rid) < 0) {
			continue;
		}

		if (test_port_common.servedCnt < TEST_PORT_SERVED_MAX) {
			test_port_common.served[test_port_common.servedCnt] = msg.type;
			test_port_common.servedCnt++;
		}

		lib_printf("test: [port.handoff] serving prio %d at %u ms\n", msg.type, (unsigned int)(proc_uptime() / 1000));

		/* hold the port long enough for everyone else to queue up */
		proc_threadSleep(100000);

		msg.o.err = EOK;
		(void)proc_respond(test_port_common.order, &msg, rid);
	}

	proc_threadEnd();
}


static void test_port_orderClient(void *arg)
{
	unsigned long delay = (unsigned long)arg;
	msg_t msg;

	proc_threadSleep(delay);

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = (int)proc_current()->priority;

	lib_printf("test: [port.handoff] prio %d sending at %u ms\n", msg.type, (unsigned int)(proc_uptime() / 1000));

	if (proc_send(test_port_common.order, &msg) < 0) {
		lib_printf("test: [port.handoff] send failed\n");
	}

	proc_threadEnd();
}


static void test_port_orderChecker(void *arg)
{
	static const int expected[] = { 13, 1, 3, 9, 12 };
	unsigned int i;

	proc_threadSleep(800000);

	lib_printf("test: [port.handoff] service order:");
	for (i = 0; i < test_port_common.servedCnt; i++) {
		lib_printf(" %d", test_port_common.served[i]);
	}
	lib_printf(" (expected: 13 1 3 9 12)\n");

	LIB_ASSERT_ALWAYS(test_port_common.servedCnt == 5,
			"HAND-OFF FAILED: %u of 5 senders served", test_port_common.servedCnt);

	for (i = 0; i < 5; i++) {
		LIB_ASSERT_ALWAYS(test_port_common.served[i] == expected[i],
				"HAND-OFF FAILED: sender %u served out of priority order (got %d, expected %d)",
				i, test_port_common.served[i], expected[i]);
	}

	lib_printf("test: [port.handoff] PASSED\n");

	proc_threadEnd();
}


static void test_port_handoffOrder(void)
{
	if (proc_portCreate(&test_port_common.order) != EOK) {
		lib_printf("test: [port.handoff] could not create a port\n");
		return;
	}

	proc_threadCreate(NULL, test_port_orderServer, NULL, 8, 2048, NULL, 0, 0, NULL);

	/*
	 * Arrival order (by delay) is deliberately not the priority order. The
	 * first sender is the one that finds the receiver parked and takes it
	 * straight away; every other one queues behind it and has to be handed
	 * the receiver in priority order.
	 */
	proc_threadCreate(NULL, test_port_orderClient, NULL, 13, 2048, NULL, 0, 0, (void *)0UL);
	proc_threadCreate(NULL, test_port_orderClient, NULL, 12, 2048, NULL, 0, 0, (void *)20000UL);
	proc_threadCreate(NULL, test_port_orderClient, NULL, 3, 2048, NULL, 0, 0, (void *)40000UL);
	proc_threadCreate(NULL, test_port_orderClient, NULL, 9, 2048, NULL, 0, 0, (void *)60000UL);
	proc_threadCreate(NULL, test_port_orderClient, NULL, 1, 2048, NULL, 0, 0, (void *)80000UL);

	proc_threadCreate(NULL, test_port_orderChecker, NULL, 0, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 2: a sender queued behind a *busy* receiver must lend it its
 * priority, the same way a thread blocking on a held mutex does.
 */


static void test_port_busyServerThr(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	unsigned int i;

	test_port_common.busyServer = proc_current();

	for (i = 0; i < 2; i++) {
		if (proc_recv(test_port_common.busy, &msg, &rid) < 0) {
			continue;
		}

		proc_threadSleep(200000);

		msg.o.err = EOK;
		(void)proc_respond(test_port_common.busy, &msg, rid);
	}

	/* stay alive (and unboosted) for the last check */
	proc_threadSleep(2000000);

	proc_threadEnd();
}


static void test_port_busyClient(void *arg)
{
	unsigned long delay = (unsigned long)arg;
	msg_t msg;

	proc_threadSleep(delay);

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = (int)proc_current()->priority;

	(void)proc_send(test_port_common.busy, &msg);

	proc_threadEnd();
}


static void test_port_busyChecker(void *arg)
{
	int prio;

	/* serving the low-priority client, nothing to inherit yet */
	proc_threadSleep(50000);
	prio = proc_threadPriority(test_port_common.busyServer, -1);
	lib_printf("test: [port.boost] server priority while serving prio 14: %d\n", prio);
	LIB_ASSERT_ALWAYS(prio == 12, "BWI FAILED: server priority changed without a boost (got %d, expected 12)", prio);

	/* prio 2 client is now queued behind the busy server */
	proc_threadSleep(100000);
	prio = proc_threadPriority(test_port_common.busyServer, -1);
	lib_printf("test: [port.boost] server priority with a prio 2 sender queued: %d\n", prio);
	LIB_ASSERT_ALWAYS(prio == 2, "BWI FAILED: busy server not boosted by a queued sender (got %d, expected 2)", prio);

	/* both served, the donation is back home */
	proc_threadSleep(400000);
	prio = proc_threadPriority(test_port_common.busyServer, -1);
	lib_printf("test: [port.boost] server priority after both were served: %d\n", prio);
	LIB_ASSERT_ALWAYS(prio == 12, "BWI FAILED: server priority not restored (got %d, expected 12)", prio);

	lib_printf("test: [port.boost] PASSED\n");

	proc_threadEnd();
}


static void test_port_busyBoost(void)
{
	if (proc_portCreate(&test_port_common.busy) != EOK) {
		lib_printf("test: [port.boost] could not create a port\n");
		return;
	}

	proc_threadCreate(NULL, test_port_busyServerThr, NULL, 12, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_port_busyClient, NULL, 14, 2048, NULL, 0, 0, (void *)0UL);
	proc_threadCreate(NULL, test_port_busyClient, NULL, 2, 2048, NULL, 0, 0, (void *)100000UL);
	proc_threadCreate(NULL, test_port_busyChecker, NULL, 0, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 3: stress. Several senders at mixed priorities against two
 * receivers of one port, some of them holding a mutex across the call while
 * another thread contends it - that chains a lock donation through a sender
 * that is itself waiting on a port. Plus pulses, which take a parked receiver
 * off the same queue the senders wait on. Driven by the assertions in
 * proc/threads.c: any invariant that breaks panics the kernel.
 */


static void test_port_stressServer(void *arg)
{
	msg_t msg;
	msg_rid_t rid;

	for (;;) {
		if (proc_recv(test_port_common.stress, &msg, &rid) < 0) {
			continue;
		}

		proc_threadSleep(200);

		msg.o.err = EOK;
		(void)proc_respond(test_port_common.stress, &msg, rid);
	}
}


static void test_port_stressClient(void *arg)
{
	unsigned long id = (unsigned long)arg;
	msg_t msg;
	unsigned int iter;

	for (iter = 0; iter < TEST_PORT_STRESS_ITERS; iter++) {
		hal_memset(&msg, 0, sizeof(msg));
		msg.type = (int)id;

		if ((id % 3UL) == 0UL) {
			/* mixed chain: a lock waiter has to relay through us */
			(void)proc_lockSet(&test_port_common.lock);
			(void)proc_send(test_port_common.stress, &msg);
			(void)proc_lockClear(&test_port_common.lock);
		}
		else {
			(void)proc_send(test_port_common.stress, &msg);
		}

		proc_threadSleep(100UL + 30UL * id);
	}

	test_port_common.clientsDone++;
	lib_printf("test: [port.stress] client %lu done\n", id);

	proc_threadEnd();
}


static void test_port_stressContender(void *arg)
{
	unsigned int iter;

	for (iter = 0; iter < TEST_PORT_STRESS_ITERS; iter++) {
		(void)proc_lockSet(&test_port_common.lock);
		proc_threadSleep(150);
		(void)proc_lockClear(&test_port_common.lock);
		proc_threadSleep(150);
	}

	lib_printf("test: [port.stress] lock contender done\n");

	proc_threadEnd();
}


static void test_port_pulseRecv(void *arg)
{
	msg_t msg;
	msg_rid_t rid;

	for (;;) {
		if (proc_recv(test_port_common.pulse, &msg, &rid) == -EPULSE) {
			test_port_common.pulses++;
		}
	}
}


static void test_port_pulseSend(void *arg)
{
	unsigned int i;

	for (i = 0; i < TEST_PORT_STRESS_PULSES; i++) {
		(void)proc_pulse(test_port_common.pulse, (u8)i);
		proc_threadSleep(2000);
	}

	proc_threadEnd();
}


static void test_port_stressChecker(void *arg)
{
	proc_threadSleep(6000000);

	lib_printf("test: [port.stress] %u of 6 clients done, %u pulses received\n",
			test_port_common.clientsDone, test_port_common.pulses);

	LIB_ASSERT_ALWAYS(test_port_common.clientsDone == 6,
			"STRESS FAILED: %u of 6 clients finished", test_port_common.clientsDone);
	LIB_ASSERT_ALWAYS(test_port_common.pulses == TEST_PORT_STRESS_PULSES,
			"STRESS FAILED: %u of %u pulses received", test_port_common.pulses, TEST_PORT_STRESS_PULSES);

	lib_printf("test: [port.stress] PASSED\n");

	proc_threadEnd();
}


static void test_port_stressRun(void)
{
	unsigned long id;

	if ((proc_portCreate(&test_port_common.stress) != EOK) || (proc_portCreate(&test_port_common.pulse) != EOK)) {
		lib_printf("test: [port.stress] could not create a port\n");
		return;
	}

	(void)proc_lockInit(&test_port_common.lock, &proc_lockAttrDefault, "test.port.lock");

	proc_threadCreate(NULL, test_port_stressServer, NULL, 10, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_port_stressServer, NULL, 11, 2048, NULL, 0, 0, NULL);

	for (id = 0; id < 6; id++) {
		proc_threadCreate(NULL, test_port_stressClient, NULL, (u8)(1 + 2 * id), 2048, NULL, 0, 0, (void *)id);
	}

	proc_threadCreate(NULL, test_port_stressContender, NULL, 2, 2048, NULL, 0, 0, NULL);

	proc_threadCreate(NULL, test_port_pulseRecv, NULL, 6, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_port_pulseSend, NULL, 7, 2048, NULL, 0, 0, NULL);

	proc_threadCreate(NULL, test_port_stressChecker, NULL, 0, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 4: teardown. A receiver is killed while it is serving one sender
 * and being boosted by others, and a sender's priority is changed while it
 * sits queued. Nobody may be stranded: a second receiver has to serve
 * everyone to the end.
 */


static void test_port_dyingServer(void *arg)
{
	msg_t msg;
	msg_rid_t rid;

	test_port_common.dying = proc_current();

	for (;;) {
		if (proc_recv(test_port_common.teardown, &msg, &rid) < 0) {
			continue;
		}

		proc_threadSleep(3000);

		msg.o.err = EOK;
		(void)proc_respond(test_port_common.teardown, &msg, rid);
	}
}


static void test_port_survivingServer(void *arg)
{
	msg_t msg;
	msg_rid_t rid;

	for (;;) {
		if (proc_recv(test_port_common.teardown, &msg, &rid) < 0) {
			continue;
		}

		proc_threadSleep(1000);

		msg.o.err = EOK;
		(void)proc_respond(test_port_common.teardown, &msg, rid);
	}
}


static void test_port_teardownClient(void *arg)
{
	unsigned long id = (unsigned long)arg;
	msg_t msg;
	unsigned int iter;

	test_port_common.client[id] = proc_current();

	for (iter = 0; iter < TEST_PORT_TEARDOWN_ITERS; iter++) {
		hal_memset(&msg, 0, sizeof(msg));
		msg.type = (int)id;

		(void)proc_send(test_port_common.teardown, &msg);
		proc_threadSleep(1000);
	}

	test_port_common.teardownDone++;
	lib_printf("test: [port.teardown] client %lu done\n", id);

	proc_threadEnd();
}


static void test_port_teardownChaos(void *arg)
{
	unsigned int iter;

	/* let the clients register themselves first */
	proc_threadSleep(20000);

	for (iter = 0; iter < 40; iter++) {
		/* re-prioritise a sender that is most likely queued right now */
		if (test_port_common.client[0] != NULL) {
			(void)proc_threadPriority(test_port_common.client[0], ((iter & 1U) != 0U) ? 14 : 4);
		}
		proc_threadSleep(3000);
	}

	if (test_port_common.dying != NULL) {
		lib_printf("test: [port.teardown] killing one of the two receivers\n");
		proc_threadDestroy(test_port_common.dying);
	}

	proc_threadEnd();
}


static void test_port_teardownChecker(void *arg)
{
	proc_threadSleep(3000000);

	lib_printf("test: [port.teardown] %u of 3 clients done\n", test_port_common.teardownDone);
	LIB_ASSERT_ALWAYS(test_port_common.teardownDone == 3,
			"TEARDOWN FAILED: %u of 3 clients finished", test_port_common.teardownDone);

	lib_printf("test: [port.teardown] PASSED\n");

	proc_threadEnd();
}


static void test_port_teardownRun(void)
{
	unsigned long id;

	if (proc_portCreate(&test_port_common.teardown) != EOK) {
		lib_printf("test: [port.teardown] could not create a port\n");
		return;
	}

	proc_threadCreate(NULL, test_port_dyingServer, NULL, 10, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_port_survivingServer, NULL, 11, 2048, NULL, 0, 0, NULL);

	for (id = 0; id < 3; id++) {
		proc_threadCreate(NULL, test_port_teardownClient, NULL, (u8)(4 + 2 * id), 2048, NULL, 0, 0, (void *)id);
	}

	proc_threadCreate(NULL, test_port_teardownChaos, NULL, 3, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_port_teardownChecker, NULL, 0, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 5: a port destroyed while threads wait in proc_recv() on it. Every
 * receiver has to be woken, and the port has to stay alive until the last one
 * has left its queue - a waiting receiver does not return through the frame
 * that looked the port up, so nothing else accounts for it.
 */


static void test_port_closeRecv(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	int err;

	test_port_common.closeWaiting++;

	err = proc_recv(test_port_common.close, &msg, &rid);
	lib_printf("test: [port.close] receiver woke with %d\n", err);

	test_port_common.closeWoken++;

	proc_threadEnd();
}


static void test_port_closeDestroyer(void *arg)
{
	while (test_port_common.closeWaiting < TEST_PORT_CLOSE_RECVS) {
		proc_threadSleep(1000);
	}

	/* the counter is bumped before proc_recv(), so give them time to block */
	proc_threadSleep(50000);

	/* otherwise the scenario passes without ever testing a close */
	LIB_ASSERT_ALWAYS(test_port_common.closeWoken == 0U,
			"CLOSE FAILED: %u receivers returned before the port was destroyed",
			test_port_common.closeWoken);

	lib_printf("test: [port.close] destroying the port\n");
	proc_portDestroy(test_port_common.close);

	proc_threadEnd();
}


static void test_port_closeChecker(void *arg)
{
	proc_threadSleep(500000);

	lib_printf("test: [port.close] %u of %u receivers woke\n",
			test_port_common.closeWoken, TEST_PORT_CLOSE_RECVS);
	LIB_ASSERT_ALWAYS(test_port_common.closeWoken == TEST_PORT_CLOSE_RECVS,
			"CLOSE FAILED: %u of %u receivers woke from proc_recv()",
			test_port_common.closeWoken, TEST_PORT_CLOSE_RECVS);

	lib_printf("test: [port.close] PASSED\n");

	proc_threadEnd();
}


static void test_port_closeRun(void)
{
	unsigned int i;

	if (proc_portCreate(&test_port_common.close) != EOK) {
		lib_printf("test: [port.close] could not create a port\n");
		return;
	}

	for (i = 0; i < TEST_PORT_CLOSE_RECVS; i++) {
		proc_threadCreate(NULL, test_port_closeRecv, NULL, 5, 2048, NULL, 0, 0, NULL);
	}

	proc_threadCreate(NULL, test_port_closeDestroyer, NULL, 6, 2048, NULL, 0, 0, NULL);
	proc_threadCreate(NULL, test_port_closeChecker, NULL, 0, 2048, NULL, 0, 0, NULL);
}


/*
 * Scenario 6: reference counting. Once every sender and receiver is gone, a
 * port has to hold exactly the references it held before any traffic - one
 * per message is what three rounds of fixes to this accounting got wrong, and
 * every one of them passed the four scenarios above.
 *
 * Two things this has to get right. A thread's queue reference is released by
 * the reaper, so the count only settles once one has run; reading it earlier
 * measures pending releases rather than leaked ones. And reading it at all
 * takes a reference of its own, which has to come back off.
 */


static int test_port_refsRead(void)
{
	port_t *p = proc_portGet(test_port_common.refs);
	int refs;

	if (p == NULL) {
		return -1;
	}

	/* discount the reference the lookup just took */
	refs = __atomic_load_n(&p->refs, __ATOMIC_ACQUIRE) - 1;
	port_put(p, 0);

	return refs;
}


static void test_port_refsServer(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	unsigned int served;

	/* bounded, so that the server is gone before the count is read */
	for (served = 0; served < (TEST_PORT_REFS_CLIENTS * TEST_PORT_REFS_ITERS); served++) {
		if (proc_recv(test_port_common.refs, &msg, &rid) < 0) {
			continue;
		}

		msg.o.err = EOK;
		(void)proc_respond(test_port_common.refs, &msg, rid);
	}

	test_port_common.refsServerDone = 1;

	proc_threadEnd();
}


static void test_port_refsClient(void *arg)
{
	msg_t msg;
	unsigned int i;

	for (i = 0; i < TEST_PORT_REFS_ITERS; i++) {
		hal_memset(&msg, 0, sizeof(msg));
		msg.type = 1;

		(void)proc_send(test_port_common.refs, &msg);
	}

	test_port_common.refsDone++;

	proc_threadEnd();
}


static void test_port_refsChecker(void *arg)
{
	unsigned int sent = TEST_PORT_REFS_CLIENTS * TEST_PORT_REFS_ITERS;
	int after;

	while ((test_port_common.refsDone < TEST_PORT_REFS_CLIENTS) || (test_port_common.refsServerDone == 0U)) {
		proc_threadSleep(10000);
	}

	/* let the reaper release what the senders and the receiver left behind */
	proc_threadSleep(1000000);

	after = test_port_refsRead();

	lib_printf("test: [port.refs] %d references before %u messages, %d after\n",
			test_port_common.refsBefore, sent, after);
	LIB_ASSERT_ALWAYS(after == test_port_common.refsBefore,
			"REFS FAILED: %d references before %u messages, %d after",
			test_port_common.refsBefore, sent, after);

	lib_printf("test: [port.refs] PASSED\n");
	lib_printf("test: [port] all scenarios passed\n");

	proc_threadEnd();
}


static void test_port_refsRun(void)
{
	unsigned long id;

	if (proc_portCreate(&test_port_common.refs) != EOK) {
		lib_printf("test: [port.refs] could not create a port\n");
		return;
	}

	test_port_common.refsBefore = test_port_refsRead();

	/* the server is the worst priority of the three, so senders have to queue */
	proc_threadCreate(NULL, test_port_refsServer, NULL, 9, 2048, NULL, 0, 0, NULL);

	for (id = 0; id < TEST_PORT_REFS_CLIENTS; id++) {
		proc_threadCreate(NULL, test_port_refsClient, NULL, (u8)(5 + id), 2048, NULL, 0, 0, (void *)id);
	}

	proc_threadCreate(NULL, test_port_refsChecker, NULL, 0, 2048, NULL, 0, 0, NULL);
}


static void test_msg_portInitthr(void *arg)
{
	/* Enable locking and multithreading related mechanisms */
	_hal_start();

	lib_printf("test: [port] starting port hand-off / BWI test suite\n");

	test_port_handoffOrder();
	proc_threadSleep(1000000);

	test_port_busyBoost();
	proc_threadSleep(1200000);

	test_port_stressRun();
	proc_threadSleep(7000000);

	test_port_teardownRun();
	proc_threadSleep(4000000);

	test_port_closeRun();
	proc_threadSleep(1500000);

	/* the reaper below has to be running for this one, see the comment there */
	test_port_refsRun();

	for (;;) {
		proc_reap();
	}
}


/* Runs the port scenarios in sequence, isolated from the rest of the boot
 * process (no syspage programs, no userspace). */
void test_msg_port(void)
{
	(void)proc_start(test_msg_portInitthr, NULL, (const char *)"porttest");

	hal_cpuEnableInterrupts();
	(void)hal_cpuReschedule(NULL, NULL);
}


/*
 * Scenario 7: kill storm. Clients call servers through one port while a
 * killer destroys random ones of them all the time, so every IPC teardown
 * path gets hit with the other side in any state. Nothing may assert, the
 * survivors have to keep making progress, and in the end every thread has
 * to be able to exit.
 *
 * Kernel threads only notice a kill by checking exit themselves, so workers
 * do it after every IPC call. Forwarding is left out: a forwarded kernel
 * thread would never consume its fastpathExitCtx.
 */


#define TEST_STORM_SERVERS 3
#define TEST_STORM_CLIENTS 6
#define TEST_STORM_SLOTS   (TEST_STORM_SERVERS + TEST_STORM_CLIENTS)
#define TEST_STORM_KILLS   3000
#define TEST_STORM_WINDOW  250


static struct {
	unsigned int port;
	int tid[TEST_STORM_SLOTS]; /* threads are only touched by tid, a kill may end them anywhere */
	volatile unsigned int calls;
	volatile unsigned int epipes;
	volatile int stop;
} test_storm;


static void test_storm_exitIfKilled(void)
{
	if ((proc_current()->exit != 0U) || (test_storm.stop != 0)) {
		proc_threadEnd();
	}
}


static void test_storm_server(void *arg)
{
	unsigned long id = (unsigned long)arg;
	unsigned int seed = (unsigned int)id * 7919U + 1U;
	msg_t msg;
	msg_rid_t rid;

	/* odd servers take small messages into their IPC buffer, the others get them mapped */
	if ((id & 1U) != 0U) {
		(void)xfer_setupIpcBuf(proc_current(), 2U * SIZE_PAGE);
	}

	for (;;) {
		test_storm_exitIfKilled();

		if (proc_recv(test_storm.port, &msg, &rid) < 0) {
			continue;
		}

		if ((lib_rand(&seed) % 3) == 0) {
			proc_threadSleep(500 + (lib_rand(&seed) % 3000));
		}

		/* killed while serving: leave without responding, teardown answers -EPIPE */
		test_storm_exitIfKilled();

		if ((msg.o.data != NULL) && (msg.o.size != 0U)) {
			hal_memset(msg.o.data, (int)id, msg.o.size);
		}
		msg.o.err = EOK;
		(void)proc_respond(test_storm.port, &msg, rid);
	}
}


static void test_storm_client(void *arg)
{
	unsigned long id = (unsigned long)arg;
	unsigned int seed = (unsigned int)id * 104729U + 3U;
	unsigned char in[64], out[64];
	msg_t msg;
	int err;

	for (;;) {
		test_storm_exitIfKilled();

		hal_memset(&msg, 0, sizeof(msg));
		msg.type = (int)id;
		if ((lib_rand(&seed) % 2) != 0) {
			msg.i.data = in;
			msg.i.size = 1U + (lib_rand(&seed) % sizeof(in));
			msg.o.data = out;
			msg.o.size = 1U + (lib_rand(&seed) % sizeof(out));
		}

		err = ((lib_rand(&seed) % 4) == 0) ? proc_send_returnable(test_storm.port, &msg) : proc_send(test_storm.port, &msg);
		if (err == EOK) {
			test_storm.calls++;
		}
		else if (err == -EPIPE) {
			test_storm.epipes++;
		}
	}
}


/* NULL once the thread is reaped, otherwise referenced until threads_put() */
static thread_t *test_storm_get(unsigned int id)
{
	return (test_storm.tid[id] != 0) ? threads_findThread(test_storm.tid[id]) : NULL;
}


static void test_storm_respawn(void)
{
	unsigned long id;
	thread_t *t;
	int tid;

	for (id = 0; id < TEST_STORM_SLOTS; id++) {
		t = test_storm_get(id);
		if (t != NULL) {
			threads_put(t);
			continue;
		}

		tid = 0;
		if (id < TEST_STORM_SERVERS) {
			(void)proc_threadCreate(NULL, test_storm_server, &tid, (u8)(8 + id), 4096, NULL, 0, 0, (void *)id);
		}
		else {
			(void)proc_threadCreate(NULL, test_storm_client, &tid, (u8)(4 + (id % 6)), 4096, NULL, 0, 0, (void *)id);
		}
		test_storm.tid[id] = tid;
	}
}


/* Kills every slot that is still there, returns how many were. */
static unsigned int test_storm_killAll(int dump)
{
	unsigned int id, n = 0;
	thread_t *t;

	for (id = 0; id < TEST_STORM_SLOTS; id++) {
		t = test_storm_get(id);
		if (t == NULL) {
			continue;
		}
		n++;
		if (dump != 0) {
			lib_printf("test: [port.storm] left %u: tid %d state %u exit %u intr %u kind %u on %p sc %p granted %p wait %p waitPort %p busyPort %p reply %p\n",
					id, proc_getTid(t), t->state, t->exit, t->interruptible, t->blockedKind, t->blockedOn, (void *)t->scActive,
					(void *)t->granted, (void *)t->wait, (void *)t->waitPort, (void *)t->busyPort, (void *)t->reply);
		}
		proc_threadDestroy(t);
		threads_put(t);
	}

	return n;
}


static void test_storm_killer(void *arg)
{
	unsigned int seed = 12345U, i, stalls = 0, lastCalls = 0, tries, left;
	thread_t *t;

	for (i = 0; i < TEST_STORM_KILLS; i++) {
		proc_threadSleep(500 + (lib_rand(&seed) % 3000));

		t = test_storm_get((unsigned int)lib_rand(&seed) % TEST_STORM_SLOTS);
		if (t != NULL) {
			proc_threadDestroy(t);
			threads_put(t);
		}

		test_storm_respawn();

		if (((i + 1U) % TEST_STORM_WINDOW) == 0U) {
			lib_printf("test: [port.storm] %u kills, %u calls, %u -EPIPE\n", i + 1U, test_storm.calls, test_storm.epipes);
			if (test_storm.calls == lastCalls) {
				stalls++;
			}
			lastCalls = test_storm.calls;
		}
	}

	/* everybody has to be able to leave, whatever they were doing */
	test_storm.stop = 1;
	left = TEST_STORM_SLOTS;
	for (tries = 0; (tries < 500U) && (left != 0U); tries++) {
		left = test_storm_killAll(0);
		proc_threadSleep(10000);
	}
	left = test_storm_killAll(1);

	lib_printf("test: [port.storm] done: %u calls, %u -EPIPE, %u stalled windows, %u threads left\n",
			test_storm.calls, test_storm.epipes, stalls, left);
	LIB_ASSERT_ALWAYS(left == 0U, "STORM FAILED: %u threads cannot exit", left);
	LIB_ASSERT_ALWAYS(stalls == 0U, "STORM FAILED: no progress in %u windows", stalls);
	/* servers are killed while serving, so some calls have to fail */
	LIB_ASSERT_ALWAYS(test_storm.epipes != 0U, "STORM FAILED: no call saw its server die");
	lib_printf("test: [port.storm] PASSED\n");

	proc_threadEnd();
}


/*
 * Scenario 8: the reaper calls a dead process. Closing the files of a dead
 * process makes the reaper call servers, and one of them can die while the
 * reaper waits for its receiver. Its ports are only closed once the reaper
 * reaps it, so the reaper must notice by itself that nobody will answer.
 */


static struct {
	unsigned int port;
	process_t *owner;
	volatile int ready;
} test_dead;


static void test_dead_owner(void *arg)
{
	/* owns the port but never receives on it */
	if (proc_portCreate(&test_dead.port) != EOK) {
		proc_threadEnd();
	}
	test_dead.owner = proc_current()->process;
	test_dead.ready = 1;

	for (;;) {
		proc_threadSleep(100000);
		if (proc_current()->exit != 0U) {
			proc_threadEnd();
		}
	}
}


static void test_dead_killer(void *arg)
{
	/* the reaper is queued on the port by now */
	proc_threadSleep(50000);
	lib_printf("test: [port.deadowner] killing the port owner\n");
	proc_kill(test_dead.owner);
	proc_threadEnd();
}


/* Runs in the reaper itself. */
static void test_dead_run(void)
{
	msg_t msg;
	int err;

	(void)proc_start(test_dead_owner, NULL, (const char *)"deadowner");
	while (test_dead.ready == 0) {
		proc_threadSleep(1000);
	}

	proc_threadCreate(NULL, test_dead_killer, NULL, 2, 2048, NULL, 0, 0, NULL);

	hal_memset(&msg, 0, sizeof(msg));
	err = proc_send_returnable(test_dead.port, &msg);

	lib_printf("test: [port.deadowner] call returned %d\n", err);
	LIB_ASSERT_ALWAYS(err < 0, "DEADOWNER FAILED: a dead port answered");
	lib_printf("test: [port.deadowner] PASSED\n");
}


static void test_msg_stormInitthr(void *arg)
{
	_hal_start();

	test_dead_run();

	if (proc_portCreate(&test_storm.port) != EOK) {
		lib_printf("test: [port.storm] could not create a port\n");
		return;
	}
	test_storm_respawn();
	proc_threadCreate(NULL, test_storm_killer, NULL, 2, 4096, NULL, 0, 0, NULL);

	for (;;) {
		proc_reap();
	}
}


void test_msg_storm(void)
{
	(void)proc_start(test_msg_stormInitthr, NULL, (const char *)"stormtest");

	hal_cpuEnableInterrupts();
	(void)hal_cpuReschedule(NULL, NULL);
}


/* parasoft-end-suppress ALL */
