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

/* Return id, allocated in msgReceive, used in msgRespond */
typedef int msg_rid_t;

/*
 * Message types
 */

/* clang-format off */

enum {
	/* File operations */
	mtOpen = 0, mtClose, mtRead, mtWrite, mtTruncate, mtDevCtl,

	/* Object operations */
	mtCreate, mtDestroy, mtSetAttr, mtGetAttr, mtGetAttrAll,

	/* Directory operations */
	mtLookup, mtLink, mtUnlink, mtReaddir,

	mtCount,

	mtStat = 0xf53 /* Moved from libphoenix. */
};

/* clang-format on */


#pragma pack(push, 8)


struct _attr {
	long long val;
	int err;
};


struct _attrAll {
	struct _attr mode;
	struct _attr uid;
	struct _attr gid;
	struct _attr size;
	struct _attr blocks;
	struct _attr ioblock;
	struct _attr type;
	struct _attr port;
	struct _attr pollStatus;
	struct _attr eventMask;
	struct _attr cTime;
	struct _attr mTime;
	struct _attr aTime;
	struct _attr links;
	struct _attr dev;
};


typedef struct _msg_t {
	int type;
	unsigned int pid;
	unsigned int priority;
	oid_t oid;

	struct {
		union {
			/* OPEN/CLOSE */
			struct {
				int flags;
			} openclose;

			/* READ/WRITE/TRUNCATE */
			struct {
				off_t offs;
				size_t len;
				unsigned mode;
			} io;

			/* CREATE */
			struct {
				int type;
				unsigned mode;
				oid_t dev;
			} create;

			/* SETATTR/GETATTR */
			struct {
				long long val;
				int type;
			} attr;

			/* LINK/UNLINK */
			struct {
				oid_t oid;
			} ln;

			/* READDIR */
			struct {
				off_t offs;
			} readdir;

			unsigned char raw[64];
		};

		size_t size;
		const void *data;
	} i;

	struct {
		union {
			/* ATTR */
			struct {
				long long val;
			} attr;

			/* CREATE */
			struct {
				oid_t oid;
			} create;

			/* LOOKUP */
			struct {
				oid_t fil;
				oid_t dev;
			} lookup;

			unsigned char raw[64];
		};

		int err;
		size_t size;
		void *data;
	} o;

} msg_t;


typedef struct {
	int err;
	size_t size;
	unsigned char raw[64];
} ipc_buf_t;


#pragma pack(pop)


#endif
