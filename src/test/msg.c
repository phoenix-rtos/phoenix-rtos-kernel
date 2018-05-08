/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for messages
 *
 * Copyright 2017 Phoenix Systems
 * Author: Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../proc/proc.h"


spinlock_t spinlock;


static void test_msg_serverthr(void *arg)
{
	char buff[64];
	u32 port = (u32) arg;
	msghdr_t hdr;
	char *prefix = NULL;

	hal_spinlockSet(&spinlock);
	lib_printf("test: [proc.msg] Start serverthr\n");
	hal_spinlockClear(&spinlock);

	for (;;) {
		proc_recv(port, buff, sizeof(buff), &hdr);

		switch (hdr.op) {
			case MSG_READ:
				prefix = "MSG_READ";
				break;
			case MSG_WRITE:
				prefix = "MSG_WRITE";
				break;
			case MSG_DEVCTL:
				prefix = "MSG_DEVCTL";
				break;
			default:
				prefix = "MSG_OTHER";
				break;
		}

		hal_spinlockSet(&spinlock);
		lib_printf("test: [proc.msg] Server %s: '%s' from %u\n", prefix, buff, hdr.sender);
		hal_spinlockClear(&spinlock);

		if (hdr.type == MSG_NORMAL)
			proc_respond(port, EOK, "Thank you!", 11);
	}
}


static void test_msg_clientthr(void *arg)
{
	char buff[64];
	size_t size = sizeof(buff);
	u32 port = (u32) arg;

	hal_spinlockSet(&spinlock);
	lib_printf("test: [proc.msg] Start clientthr\n");
	hal_spinlockClear(&spinlock);

	for (;;) {
		proc_send(port, MSG_WRITE, "Have a nice day!", 17, MSG_NORMAL, buff, size);
		hal_spinlockSet(&spinlock);
		lib_printf("test: [proc.msg] Client: '%s'\n", buff);
		hal_spinlockClear(&spinlock);

		proc_threadSleep(1000000);
	}
}


void test_msg(void)
{
	u32 port;

	if (proc_portCreate(&port) != EOK) {
		lib_printf("Failed to create port\n");
		hal_cpuHalt();
	}

	hal_spinlockCreate(&spinlock, "test.msg");

	proc_threadCreate(NULL, test_msg_serverthr, NULL, 1, 512, NULL, 0, (void *) port);
	proc_threadCreate(NULL, test_msg_clientthr, NULL, 1, 512, NULL, 0, (void *) port);
}
