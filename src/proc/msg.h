/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Ports and messages
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_MSG_H_
#define _PROC_MSG_H_

#include HAL
#include "../../include/errno.h"
#include "../../include/msg.h"


typedef struct _kmsg_t {
	msg_t msg;

	struct _kmsg_t *next;
	struct _kmsg_t *prev;
	thread_t *threads;
	process_t *src;
	volatile int responded;
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
#endif
} kmsg_t;


/*
 * Port management
 */


extern int proc_portCreate(u32 *port);


extern void proc_portDestroy(u32 port);


/*extern int proc_portRegister(u32 port, char *name);


extern void proc_portUnregister(char *name);


extern int proc_portLookup(char *name, unsigned int *port);*/


extern void proc_portsDestroy(process_t *proc);


/*
 * Message passing
 */


extern int proc_send(u32 port, msg_t *msg);


extern int proc_recv(u32 port, msg_t *msg, unsigned int *rid);


extern int proc_respond(u32 port, msg_t *msg, unsigned int rid);


extern void _msg_init(vm_map_t *kmap, vm_object_t *kernel);


#endif
