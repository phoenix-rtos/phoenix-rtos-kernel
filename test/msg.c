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

#include "hal/hal.h"
#include "include/errno.h"
#include "proc/proc.h"


unsigned test_randsize(unsigned *seed, unsigned bufsz)
{
	unsigned sz;

	if (lib_rand(seed) % 2) {
		sz = (lib_rand(seed) % (bufsz / SIZE_PAGE)) * SIZE_PAGE;
	}
	else {
		sz = 1 + (lib_rand(seed) % bufsz);
	}

	return (sz != 0) ? sz : 1;
}


unsigned test_offset(unsigned *seed, unsigned size, unsigned bufsz)
{
	unsigned offs = (bufsz - size) / SIZE_PAGE;

	if (offs != 0 && lib_rand(seed) % 2 != 0) {
		offs = (lib_rand(seed) % offs) * SIZE_PAGE;
	}
	else if (offs  != 0 && lib_rand(seed) % 10 != 0) {
		offs = SIZE_PAGE - (size & (SIZE_PAGE - 1));
	}
	else if (offs  != 0 && lib_rand(seed) % 10 != 0) {
		offs = SIZE_PAGE - (size & (SIZE_PAGE - 1)) / 2;
	}
	else if (bufsz - size != 0) {
		offs = lib_rand(seed) % (bufsz - size);
	}
	else {
		offs = 0;
	}
	return offs;
}


void test_ping(void *arg)
{
	msg_t msg;
	unsigned bufsz = 4 * SIZE_PAGE, offs[2], i, k;
	void *buf[2];
	unsigned int seed = (long)test_ping;
	unsigned int count = 0;
	unsigned int port = (long)arg;


	lib_printf("test: msg/ping: starting\n");

	buf[0] = vm_kmalloc(bufsz);
	buf[1] = vm_kmalloc(bufsz);

	if (buf[0] == NULL || buf[1] == NULL) {
		lib_printf("test_msg/ping: could not allocate buffers\n");
		return;
	}

	for (k = 0; count == 0 || k < count; ++k) {
		lib_printf("\rtest_msg/ping: % 20d OK", k);

		hal_memset(&msg, 0, sizeof(msg));

		msg.o.size = msg.i.size = test_randsize(&seed, bufsz);

		msg.i.data = buf[0] + (offs[0] = test_offset(&seed, msg.i.size, bufsz));
		msg.o.data = buf[1] + (offs[1] = test_offset(&seed, msg.o.size, bufsz));

		for (i = 0; i < msg.o.size; ++i)
			((unsigned char *)msg.i.data)[i] = (unsigned char)lib_rand(&seed);

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
	unsigned int port = (long)arg;

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
		else
			hal_memcpy(msg.o.data, msg.i.data, msg.i.size);

		proc_respond(port, &msg, rid);
	}

	return;
}


void test_msg(void)
{
	unsigned port;

	if (proc_portCreate(&port) != EOK) {
		lib_printf("Failed to create port\n");
		hal_cpuHalt();
	}

	proc_threadCreate(NULL, test_pong, NULL, 4, 1024, NULL, 0, (void *)(long)port);
	proc_threadCreate(NULL, test_ping, NULL, 4, 1024, NULL, 0, (void *)(long)port);
}
