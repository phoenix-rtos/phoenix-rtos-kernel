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


static int usrv_oidGet(const msg_t *msg, oid_t *oid)
{
	int err = EOK;
	ioctl_in_t *ioctl;

	switch (msg->type) {
		case mtOpen:
		case mtClose:
			oid->id = msg->i.openclose.oid.id;
			break;

		case mtRead:
		case mtWrite:
		case mtTruncate:
			oid->id = msg->i.io.oid.id;
			break;

		case mtCreate:
			oid->id = msg->i.create.dir.id;
			break;

		case mtDestroy:
			oid->id = msg->i.destroy.oid.id;
			break;

		case mtGetAttr:
		case mtSetAttr:
			oid->id = msg->i.attr.oid.id;
			break;

		case mtLookup:
			oid->id = msg->i.lookup.dir.id;
			break;

		case mtDevCtl:
			ioctl = (ioctl_in_t *)msg->i.raw;
			oid->id = ioctl->id;
			break;

		default:
			err = -EINVAL;
			break;
	}

	return err;
}


static void usrv_msgthr(void *arg)
{
	msg_t msg;
	msg_rid_t rid;
	oid_t oid = usrv_common.oid;

	for (;;) {
		/* TODO: when the oid_t is added to the msg_t then usrv_oidGet should be removed */
		if (proc_recv(oid.port, &msg, &rid) != 0 || usrv_oidGet(&msg, &oid) != 0) {
			continue;
		}

		switch (oid.id) {
			case USRV_ID_LOG:
				log_msgHandler(&msg, oid, rid);
				break;

			default:
				msg.o.io.err = -ENOSYS;
				proc_respond(oid.port, &msg, rid);
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

	proc_threadCreate(NULL, usrv_msgthr, NULL, 4, 2048, NULL, 0, NULL);
}


void _usrv_init(void)
{
	_log_init();
}
