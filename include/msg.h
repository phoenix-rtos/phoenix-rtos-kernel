/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_MSG_H_
#define _PHOENIX_MSG_H_

#include "types.h"

/*
 * Message types
 */


enum {
	/* File operations */
	mtOpen = 0, mtClose, mtRead, mtWrite, mtTruncate, mtDevCtl,

	/* Object operations */
	mtCreate, mtDestroy, mtSetAttr, mtGetAttr,

	/* Directory operations */
	mtLookup, mtLink, mtUnlink, mtReaddir,

	/* Socket operations */
	mtAccept, mtBind, mtConnect, mtGetPeerName, mtGetSockName, mtListen,
	mtRecv, mtSend, mtSocket, mtShutdown, mtSetOpt, mtGetOpt, mtGetFl, mtSetFl, mtGetAddrInfo, mtGetNameInfo,

	mtCount
} type;


/* mt{Get,Set}Attr types */
enum { atMode, atUid, atGid, atSize, atType, atPort, atEvents, atCTime, atMTime, atATime, atLinks, atDev, atStatStruct };


#pragma pack(push, 8)

typedef struct _msg_t {
	short type;
	short priority;
	pid_t pid;
	int error;
	id_t object;

	struct {
		union {
			/* LOOKUP */
			struct {
				int flags;
				mode_t mode;
			} lookup;

			/* READ/WRITE */
			struct {
				off_t offs;
				unsigned flags;
			} io;

			/* GETATTR/SETATTR */
			int attr;

			/* LINK */
			oid_t link;

			/* DEVCTL */
			unsigned long devctl;

			unsigned char raw[64];
		};

		size_t size;
		const void *data;
	} i;

	struct {
		union {
			struct {
				id_t id;
				mode_t mode;
			} lookup;

			ssize_t io;

			unsigned char raw[64];
		};

		size_t size;
		void *data;
	} o;
} msg_t;


#pragma pack(pop)


#endif
