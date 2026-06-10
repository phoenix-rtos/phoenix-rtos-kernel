/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_PROC_MSG_H_
#define _PH_PROC_MSG_H_

#include "hal/hal.h"
#include "include/errno.h"
#include "include/msg.h"
#include "lib/lib.h"
#include "threads.h"


/*
 * Message passing
 */


int proc_send(u32 port, msg_t *msg);


int proc_send_returnable(u32 port, msg_t *msg);


int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid);


int proc_respond(u32 port, msg_t *msg, msg_rid_t rid);


int proc_pulse(u32 port, u8 pulse);


int proc_respondAndRecv(u32 port, msg_t *msg, msg_rid_t *rid);


void _msg_init(vm_map_t *kmap, vm_object_t *kernel);


#endif
