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

int proc_objectLookup(struct _port_t *port, id_t id, const char *name, size_t namelen, int flags, id_t *object, mode_t *mode, const oid_t *dev)
{
	msg_t msg;
	int error;

	msg.type = mtLookup;
	msg.object = id;

	msg.i.lookup.flags = flags;
	msg.i.lookup.mode = *mode;

	if (dev != NULL)
		hal_memcpy(&msg.i.lookup.dev, dev, sizeof(*dev));

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
	msg.i.io.flags = 0; /* TODO: pass flags */

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
	msg.i.io.flags = 0; /* TODO: pass flags */

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


int proc_objectMount(port_t *dev, id_t id, unsigned int port, oid_t *dir, const char *type, int flags, id_t *newid)
{
	int retval;
	msg_t msg;
	msg.type = mtMount;
	msg.object = id;

	msg.i.mount.port = port;
	msg.i.mount.dir = *dir;
	msg.i.mount.flags = flags;

	msg.i.data = type;
	msg.i.size = hal_strlen(type);

	if ((retval = port_send(dev, &msg)) < 0)
		return retval;

	*newid = msg.o.mount;
	return msg.error;
}


int proc_objectBind(struct _port_t *port, id_t id, const struct sockaddr *address, socklen_t length)
{
	msg_t msg;
	int error;

	msg.type = mtBind;
	msg.object = id;

	msg.i.size = length;
	msg.i.data = address;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}


int proc_objectAccept(struct _port_t *port, id_t id, id_t *new, const struct sockaddr *address, socklen_t *length)
{
	msg_t msg;
	int error;

	msg.type = mtAccept;
	msg.object = id;

	msg.i.size = 0;
	msg.i.data = NULL;

	if (length != NULL && address != NULL) {
		msg.o.size = *length;
		msg.o.data = address;
	}
	else {
		msg.o.size = 0;
		msg.o.data = NULL;
	}

	if ((error = port_send(port, &msg)) < 0)
		return error;

	if (length != NULL && address != NULL) {
		*length = msg.o.accept.length;
	}

	*new = msg.o.accept.id;
	return msg.error;
}


int proc_objectListen(struct _port_t *port, id_t id, int backlog)
{
	msg_t msg;
	int error;

	msg.type = mtListen;
	msg.object = id;

	msg.i.listen = backlog;

	msg.i.size = 0;
	msg.i.data = NULL;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}


int proc_objectConnect(struct _port_t *port, id_t id, const struct sockaddr *address, socklen_t length)
{
	msg_t msg;
	int error;

	msg.type = mtConnect;
	msg.object = id;

	msg.i.size = length;
	msg.i.data = address;
	msg.o.size = 0;
	msg.o.data = NULL;

	if ((error = port_send(port, &msg)) < 0)
		return error;

	return msg.error;
}
