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

int proc_objectLookup(struct _port_t *port, id_t id, const char *name, size_t namelen, int flags, id_t *object, mode_t *mode)
{
	msg_t msg;
	int error;

	msg.type = mtLookup;
	msg.object = id;

	msg.i.lookup.flags = flags;
	msg.i.lookup.mode = *mode;

	msg.i.data = name;
	msg.i.size = namelen;
	msg.o.data = NULL;
	msg.o.size = 0;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	*object = msg.o.lookup.id;
	*mode = msg.o.lookup.mode;
	return msg.error;
}


int proc_objectOpen(struct _port_t *port, id_t *id)
{
	msg_t msg;
	int error;

	msg.type = mtOpen;
	msg.object = *id;

	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	if (msg.error == EOK)
		*id = msg.o.open;

	return msg.error;
}


ssize_t proc_objectWrite(struct _port_t *port, id_t id, const void *data, size_t size, off_t offset)
{
	msg_t msg;
	int error;

	msg.type = mtWrite;
	msg.object = id;

	msg.i.io.offs = offset;

	msg.i.size = size;
	msg.i.data = data;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	if (msg.error)
		return msg.error;
	return msg.o.io;
}


ssize_t proc_objectRead(struct _port_t *port, id_t id, void *data, size_t size, off_t offset)
{
	msg_t msg;
	int error;

	msg.type = mtRead;
	msg.object = id;

	msg.i.io.offs = offset;

	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = size;
	msg.o.data = data;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	if (msg.error)
		return msg.error;
	return msg.o.io;
}


ssize_t proc_objectGetAttr(struct _port_t *port, id_t id, int attr, void *data, size_t size)
{
	msg_t msg;
	int error;

	msg.type = mtGetAttr;
	msg.object = id;

	msg.i.attr = attr;

	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = size;
	msg.o.data = data;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}


ssize_t proc_objectSetAttr(struct _port_t *port, id_t id, int attr, const void *data, size_t size)
{
	msg_t msg;
	int error;

	msg.type = mtSetAttr;
	msg.object = id;

	msg.i.attr = attr;

	msg.i.size = size;
	msg.i.data = data;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}


int proc_objectLink(struct _port_t *port, id_t id, const char *name, const oid_t *file)
{
	msg_t msg;
	int error;

	msg.type = mtLink;
	msg.object = id;

	hal_memcpy(&msg.i.link, file, sizeof(*file));

	msg.i.size = hal_strlen(name);
	msg.i.data = name;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}


int proc_objectUnlink(struct _port_t *port, id_t id, const char *name)
{
	msg_t msg;
	int error;

	msg.type = mtUnlink;
	msg.object = id;

	msg.i.size = hal_strlen(name);
	msg.i.data = name;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}


int proc_objectControl(struct _port_t *port, id_t id, unsigned command, const void *in, size_t insz, void *out, size_t outsz)
{
	msg_t msg;
	int error;

	msg.type = mtDevCtl;
	msg.object = id;

	msg.i.devctl = command;

	msg.i.size = insz;
	msg.i.data = in;
	msg.o.size = outsz;
	msg.o.data = out;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	if (msg.error)
		return msg.error;
	return msg.o.io;
}


int proc_objectClose(struct _port_t *port, id_t id)
{
	msg_t msg;
	int error;

	msg.type = mtClose;
	msg.object = id;

	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}



