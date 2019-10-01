/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Processes management
 *
 * Copyright 2012-2015, 2017 Phoenix Systems
 * Copyright 2001, 2006-2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej, Jacek Popko
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_PROC_H_
#define _PROC_PROC_H_

#include HAL
#include "threads.h"
#include "process.h"
#include "lock.h"
#include "msg.h"
#include "name.h"
#include "resource.h"
#include "mutex.h"
#include "cond.h"
#include "file.h"
#include "userintr.h"
#include "ports.h"

/* TODO: move */

int proc_objectLookup(const oid_t *oid, const char *name, size_t namelen, int flags, id_t *object, mode_t *mode);

ssize_t proc_objectWrite(const oid_t *oid, const char *data, size_t size, off_t offset);

ssize_t proc_objectRead(const oid_t *oid, char *data, size_t size, off_t offset);

ssize_t proc_objectGetAttr(const oid_t *oid, int attr, void *data, size_t size);

ssize_t proc_objectSetAttr(const oid_t *oid, int attr, const void *data, size_t size);

int proc_objectControl(const oid_t *oid, unsigned command, const void *in, size_t insz, void *out, size_t outsz);

int proc_objectLink(const oid_t *oid, const char *name, const oid_t *file);

int proc_objectUnlink(const oid_t *oid, const char *name);

void proc_groupLeave(process_t *process);

int proc_groupInit(process_t *process, process_t *parent);


typedef struct _evqueue_t evqueue_t;
extern evqueue_t *queue_create(process_t *process);


extern int _proc_init(vm_map_t *kmap, vm_object_t *kernel);


#endif
