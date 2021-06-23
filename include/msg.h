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

	mtCount
} type;


#pragma pack(push, 8)


typedef struct _msg_t {
	int type;
	unsigned int pid;
	unsigned int priority;

	struct {
		union {
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
				unsigned mode;
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
		void *data;
	} i;

	struct {
		union {
			struct {
				int err;
			} io;

			/* ATTR */
			struct {
				int val;
				int err;
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
