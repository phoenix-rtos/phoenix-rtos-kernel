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

#ifndef _PROC_MSG_H_
#define _PROC_MSG_H_

#include "hal/hal.h"
#include "include/errno.h"
#include "include/msg.h"
#include "lib/lib.h"
#include "threads.h"


typedef struct _kmsg_t {
#ifndef NOMMU
	msg_t msg;
#else
	msg_t *msg;
#endif

	struct _kmsg_t *next;
	struct _kmsg_t *prev;

	idnode_t idlinkage;

	thread_t *threads;
	process_t *src;
	volatile int state;

#ifndef NOMMU
	struct _kmsg_layout_t {
		void *bvaddr;
		u64 boffs;
		void *w;
		page_t *bp;

		void *evaddr;
		u64 eoffs;
		page_t *ep;
	} i, o;
#else
	void *imapped;
	void *omapped;
#endif
} kmsg_t;


typedef struct _fmsg_t {
	struct _fmsg_t *next, *prev;
	idnode_t idlinkage;
	thread_t *sender;
	thread_t *threads;
	volatile int state;
} fmsg_t;


/*
 * Message passing
 */


extern void proc_freeUtcb(thread_t *t);


extern msgBuf_t *proc_initMsgBuf(void);


extern int proc_send(u32 port, msg_t *msg);


extern int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid);


extern int proc_respond(u32 port, msg_t *msg, msg_rid_t rid);


extern int proc_call(u32 port);


extern int proc_respond2(u32 port);


extern int proc_recv2(u32 port);


extern int proc_respondAndRecv(u32 port);


extern void _msg_init(vm_map_t *kmap, vm_object_t *kernel);


#endif
