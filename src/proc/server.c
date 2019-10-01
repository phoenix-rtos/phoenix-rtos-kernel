/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Server api
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "proc.h"

int proc_objectLookup(const oid_t *oid, const char *name, size_t namelen, int flags, id_t *object, mode_t *mode)
{
	msg_t msg;
	int error;

	msg.type = mtOpen;
	msg.object = oid->id;

	msg.i.open_.flags = flags;
	msg.i.open_.mode = *mode;

	msg.i.data = name;
	msg.i.size = namelen;
	msg.o.data = NULL;
	msg.o.size = 0;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	*object = msg.o.open_.id;
	*mode = msg.o.open_.mode;
	return msg.o.open_.err;
}


ssize_t proc_objectWrite(const oid_t *oid, const char *data, size_t size, off_t offset)
{
	msg_t msg;
	int error;

	msg.type = mtWrite;
	msg.object = oid->id;

	msg.i.io_.offs = offset;

	msg.i.size = size;
	msg.i.data = data;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	return msg.o.io_;
}


ssize_t proc_objectRead(const oid_t *oid, char *data, size_t size, off_t offset)
{
	msg_t msg;
	int error;

	msg.type = mtRead;
	msg.object = oid->id;

	msg.i.io_.offs = offset;

	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = size;
	msg.o.data = data;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	return msg.o.io_;
}


ssize_t proc_objectGetAttr(const oid_t *oid, int attr, void *data, size_t size)
{
	msg_t msg;
	int error;

	msg.type = mtGetAttr;
	msg.object = oid->id;

	msg.i.attr_ = attr;

	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = size;
	msg.o.data = data;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	return msg.o.io_;
}


ssize_t proc_objectSetAttr(const oid_t *oid, int attr, const void *data, size_t size)
{
	msg_t msg;
	int error;

	msg.type = mtSetAttr;
	msg.object = oid->id;

	msg.i.attr_ = attr;

	msg.i.size = size;
	msg.i.data = data;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	return msg.o.io_;
}


int proc_objectLink(const oid_t *oid, const char *name, const oid_t *file)
{
	msg_t msg;
	int error;

	msg.type = mtLink;
	msg.object = oid->id;

	hal_memcpy(&msg.i.link_, file, sizeof(*file));

	msg.i.size = hal_strlen(name);
	msg.i.data = name;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	return msg.o.io_;
}


int proc_objectUnlink(const oid_t *oid, const char *name)
{
	msg_t msg;
	int error;

	msg.type = mtUnlink;
	msg.object = oid->id;

	msg.i.size = hal_strlen(name);
	msg.i.data = name;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	return msg.o.io_;
}


int proc_objectControl(const oid_t *oid, unsigned command, const void *in, size_t insz, void *out, size_t outsz)
{
	msg_t msg;
	int error;

	msg.type = mtDevCtl;
	msg.object = oid->id;

	msg.i.devctl_= command;

	msg.i.size = insz;
	msg.i.data = in;
	msg.o.size = outsz;
	msg.o.data = out;

	if ((error = proc_send(oid->port, &msg)) < 0)
		return error;

	return msg.o.io_;
}


