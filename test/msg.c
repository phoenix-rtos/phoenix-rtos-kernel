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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* parasoft-begin-suppress ALL "tests don't need to comply with MISRA" */

#include "hal/hal.h"
#include "include/errno.h"
#include "proc/proc.h"


unsigned test_randsize(unsigned *seed, unsigned bufsz)
{
	unsigned sz;

	if (lib_rand(seed) % 2) {
		sz = ((unsigned int)lib_rand(seed) % (bufsz / SIZE_PAGE)) * SIZE_PAGE;
	}
	else {
		sz = 1U + ((unsigned int)lib_rand(seed) % bufsz);
	}

	return (sz != 0U) ? sz : 1U;
}


unsigned test_offset(unsigned *seed, unsigned size, unsigned bufsz)
{
	unsigned offs = (bufsz - size) / SIZE_PAGE;

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
	unsigned bufsz = 4U * SIZE_PAGE, offs[2], i, k;
	void *buf[2];
	unsigned int seed = (unsigned long)test_ping;
	unsigned int count = 0;
	unsigned int port = (unsigned long)arg;


	/* MISRA Rule 17.7: Unused return value, (void) added in lines 70, 76, 81, 95, 100, 105, 129*/
	(void)lib_printf("test: msg/ping: starting\n");

	buf[0] = vm_kmalloc(bufsz);
	buf[1] = vm_kmalloc(bufsz);

	if (buf[0] == NULL || buf[1] == NULL) {
		(void)lib_printf("test_msg/ping: could not allocate buffers\n");
		return;
	}

	for (k = 0; count == 0U || k < count; ++k) {
		(void)lib_printf("\rtest_msg/ping: % 20d OK", k);

		hal_memset(&msg, 0, sizeof(msg));

		msg.o.size = msg.i.size = test_randsize(&seed, bufsz);

		msg.i.data = buf[0] + (offs[0] = test_offset(&seed, msg.i.size, bufsz));
		msg.o.data = buf[1] + (offs[1] = test_offset(&seed, msg.o.size, bufsz));

		for (i = 0; i < msg.o.size; ++i) {
			((unsigned char *)msg.i.data)[i] = (unsigned char)lib_rand(&seed);
		}

		if (proc_send(port, &msg) < 0) {
			(void)lib_printf("\ntest_msg/ping: send failed\n");
			return;
		}

		if (msg.o.err < 0) {
			(void)lib_printf("\ntest_msg/ping: pong returned error\n");
			return;
		}

		if (msg.i.size != msg.o.size) {
			(void)lib_printf("\ntest_msg/ping: sizes mismatch\n");
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

	(void)lib_printf("\n");

	return;
}


void test_pong(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	unsigned int port = (unsigned long)arg;

	/* MISRA Rule 17.7: Unused return value, (void) added in lines 142, 147, 152, 159*/
	(void)lib_printf("test_msg/pong: starting\n");

	for (;;) {

		if (proc_recv(port, &msg, &rid) < 0) {
			(void)lib_printf("test_msg/pong: receive failed\n");
			msg.o.err = 1;
		}

		if (msg.i.size != msg.o.size) {
			(void)lib_printf("test_msg/pong: i/o buffers are of different sizes: 0x%zx and 0x%zx\n", msg.i.size, msg.o.size);
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
	unsigned port;

	if (proc_portCreate(&port) != EOK) {
		/* MISRA Rule 17.7: Unused return value, (void) added in lines 172, 176, 177*/
		(void)lib_printf("Failed to create port\n");
		hal_cpuHalt();
	}

	(void)proc_threadCreate(NULL, test_pong, NULL, 4, 1024, NULL, 0, (void *)(long)port);
	(void)proc_threadCreate(NULL, test_ping, NULL, 4, 1024, NULL, 0, (void *)(long)port);
}


/* parasoft-end-suppress ALL */
