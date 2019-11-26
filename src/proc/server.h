/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Server API
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_SERVER_H_
#define _PROC_SERVER_H_

#include HAL
#include "ports.h"

extern int proc_objectLookup(struct _port_t *port, id_t id, const char *name, size_t namelen, int flags, id_t *object, mode_t *mode, const oid_t *dev);


extern int proc_objectOpen(struct _port_t *port, id_t *id);


extern ssize_t proc_objectWrite(struct _port_t *port, id_t id, const void *data, size_t size, off_t offset);


extern ssize_t proc_objectRead(struct _port_t *port, id_t id, void *data, size_t size, off_t offset);


extern ssize_t proc_objectGetAttr(struct _port_t *port, id_t id, int attr, void *data, size_t size);


extern ssize_t proc_objectSetAttr(struct _port_t *port, id_t id, int attr, const void *data, size_t size);


extern int proc_objectControl(struct _port_t *port, id_t id, unsigned command, const void *in, size_t insz, void *out, size_t outsz);


extern int proc_objectLink(struct _port_t *port, id_t id, const char *name, const oid_t *file);


extern int proc_objectUnlink(struct _port_t *port, id_t id, const char *name);


extern int proc_objectMount(struct _port_t *port, id_t id, unsigned int newport, const char *type, id_t *newid, mode_t *mode);


extern int proc_objectClose(struct _port_t *port, id_t id);


extern int proc_objectBind(struct _port_t *port, id_t id, const struct sockaddr *address, socklen_t length);


extern int proc_objectAccept(struct _port_t *port, id_t id, id_t *new, const struct sockaddr *address, socklen_t *length);


extern int proc_objectListen(struct _port_t *port, id_t id, int backlog);


extern int proc_objectConnect(struct _port_t *port, id_t id, const struct sockaddr *address, socklen_t length);


extern int proc_objectShutdown(struct _port_t *port, id_t id, int how);

#endif