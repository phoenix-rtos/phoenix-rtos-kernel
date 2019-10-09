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

extern int proc_objectLookup(const oid_t *oid, const char *name, size_t namelen, int flags, id_t *object, mode_t *mode);


extern ssize_t proc_objectWrite(const oid_t *oid, const char *data, size_t size, off_t offset);


extern ssize_t proc_objectRead(const oid_t *oid, char *data, size_t size, off_t offset);


extern ssize_t proc_objectGetAttr(const oid_t *oid, int attr, void *data, size_t size);


extern ssize_t proc_objectSetAttr(const oid_t *oid, int attr, const void *data, size_t size);


extern int proc_objectControl(const oid_t *oid, unsigned command, const void *in, size_t insz, void *out, size_t outsz);


extern int proc_objectLink(const oid_t *oid, const char *name, const oid_t *file);


extern int proc_objectUnlink(const oid_t *oid, const char *name);


extern int proc_objectClose(const oid_t *oid);

#endif