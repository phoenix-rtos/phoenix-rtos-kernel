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

#include "lib/lib.h"

#include "proc/threads.h"
#include "proc/msg.h"
#include "proc/ports.h"


#define USRV_ID_LOG 0


static struct {
	oid_t oid;
} usrv_common;


static void usrv_msgthr(void *arg)
{
	oid_t oid = usrv_common.oid;

	msgBuf_t *msg = proc_initMsgBuf();
	LIB_ASSERT(msg != NULL, "heh");

	void *reply;

	msgHeader_t hdr;

	for (;;) {
		reply = proc_recv2(oid.port);
		if (reply == NULL) {
			lib_debug_printf("null?\n");
			continue;
		}

		oid.id = msg->oid.id;

		switch (oid.id) {
			case USRV_ID_LOG:
				log_msgHandler2(msg, oid, reply);
				break;

			default:
				msg->err = -ENOSYS;
				proc_respond2(oid.port, reply);
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

	proc_threadCreate(NULL, usrv_msgthr, NULL, 3, SIZE_KSTACK, NULL, 0, NULL);
}


void _usrv_init(void)
{
	_log_init();
}
