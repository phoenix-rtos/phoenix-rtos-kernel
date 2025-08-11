/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * User server
 *
 * Copyright 2022 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"

#include "usrv.h"
#include "log/log.h"
#include "include/ioctl.h"

#include "proc/threads.h"
#include "proc/msg.h"
#include "proc/ports.h"


#define USRV_ID_LOG 0


static struct {
	oid_t oid;
} usrv_common;


static void usrv_msgthr(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	oid_t oid = usrv_common.oid;

	for (;;) {
		if (proc_recv(oid.port, &msg, &rid) != 0) {
			continue;
		}

		oid.id = msg.oid.id;

		switch (oid.id) {
			case USRV_ID_LOG:
				log_msgHandler(&msg, oid, rid);
				break;

			default:
				msg.o.err = -ENOSYS;
				/* MISRA Rule 17.7: Unused returned value, (void) added*/
				(void)proc_respond(oid.port, &msg, rid);
				break;
		}
	}
}


void _usrv_start(void)
{
	/* Create port 0 for /dev/kmsg */
	if (proc_portCreate(&usrv_common.oid.port) != 0) {
		return;
	}

	/* MISRA Rule 17.7: Unused returned value, (void) added*/
	(void)proc_threadCreate(NULL, usrv_msgthr, NULL, 1, SIZE_KSTACK, NULL, 0, NULL);
}


void _usrv_init(void)
{
	_log_init();
}
