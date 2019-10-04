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
enum { atSize, atStatStruct, atEvents };


#pragma pack(push, 8)

typedef struct _msg_t {
	short type;
	short priority;
	pid_t pid;
	id_t object;

	struct {
		union {
			/* OPEN */
			struct {
				int flags;
				mode_t mode;
			} open_;

			/* READ/WRITE */
			struct {
				off_t offs;
				unsigned flags;
			} io_;

			/* GETATTR/SETATTR */
			int attr_;

			/* LINK */
			oid_t link_;

			unsigned devctl_;


			/* OPEN/CLOSE */
			struct {
				oid_t oid;
				int flags;
			} openclose;

			/* READ/WRITE/TRUNCATE */
			struct {
				oid_t oid;
				offs_t offs;
				size_t len;
				unsigned mode;
			} io;

			/* CREATE */
			struct {
				oid_t dir;
				int type;
				u32 mode;
				oid_t dev;
			} create;

			/* DESTROY */
			struct {
				oid_t oid;
			} destroy;

			/* SETATTR/GETATTR */
			struct {
				oid_t oid;
				int type;
				int val;
			} attr;

			/* LOOKUP */
			struct {
				oid_t dir;
			} lookup;

			/* LINK/UNLINK */
			struct {
				oid_t dir;
				oid_t oid;
			} ln;

			/* READDIR */
			struct {
				oid_t dir;
				offs_t offs;
			} readdir;

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
				int err;
			} open_;

			ssize_t io_;


			struct {
				int err;
			} io;

			/* ATTR */
			struct {
				int val;
			} attr;

			/* CREATE */
			struct {
				oid_t oid;
				int err;
			} create;

			/* LOOKUP */
			struct {
				oid_t fil;
				oid_t dev;
				int err;
			} lookup;

			unsigned char raw[64];
		};

		size_t size;
		void *data;
	} o;
} msg_t;


#pragma pack(pop)


#endif
