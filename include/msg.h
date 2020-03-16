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
#include "socket.h"

/*
 * Message types
 */


enum {
	/* File operations */
	mtOpen = 0, mtClose, mtRead, mtWrite, mtDevCtl, mtSetAttr, mtGetAttr,

	/* Directory operations */
	mtLookup, mtLink, mtUnlink, mtMount,

	/* Socket operations */
	mtAccept, mtBind, mtConnect, mtListen,

	mtGetPeerName, mtGetSockName,
	mtRecv, mtSend, mtSocket, mtShutdown, mtSetOpt, mtGetOpt, mtGetFl, mtSetFl, mtGetAddrInfo, mtGetNameInfo,

	mtRaw,

	mtCount
} type;


/* mt{Get,Set}Attr types */
enum { atMode, atUid, atGid, atSize, atType, atPort, atEvents, atCTime, atMTime, atATime, atLinks, atDev, atStatStruct, atMount, atMountPoint,
	atLocalAddr, atRemoteAddr };


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
				oid_t dev;
			} lookup;

			/* READ/WRITE */
			struct {
				off_t offs;
				unsigned flags;
			} io;

			/* SHUTDOWN */
			int shutdown;

			/* GETATTR/SETATTR */
			int attr;

			/* LINK */
			oid_t link;

			/* MOUNT */
			struct {
				unsigned int port;
			} mount;

			/* DEVCTL */
			unsigned long devctl;

			int listen; /* backlog */

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

			id_t open;

			ssize_t io;

			struct {
				id_t id;
				socklen_t length;
			} accept;

			struct {
				id_t id;
				mode_t mode;
			} mount;

			unsigned char raw[64];
		};

		size_t size;
		void *data;
	} o;
} msg_t;


#pragma pack(pop)


#endif
