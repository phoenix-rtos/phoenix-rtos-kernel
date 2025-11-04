/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Names resolving
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_NAME_H_
#define _PROC_NAME_H_

#include "hal/hal.h"


int proc_portRegister(u32 port, const char *name, oid_t *oid);


void proc_portUnregister(const char *name);


int proc_portLookup(const char *name, oid_t *file, oid_t *dev);


int proc_lookup(const char *name, oid_t *file, oid_t *dev);


int proc_read(oid_t oid, off_t offs, void *buf, size_t sz, unsigned int mode);


int proc_link(oid_t dir, oid_t oid, const char *name);


int proc_unlink(oid_t dir, oid_t oid, const char *name);


int proc_create(u32 port, int type, unsigned int mode, oid_t dev, oid_t dir, char *name, oid_t *oid);


int proc_close(oid_t oid, unsigned int mode);


int proc_open(oid_t oid, unsigned int mode);


off_t proc_size(oid_t oid);


int proc_write(oid_t oid, off_t offs, void *buf, size_t sz, unsigned int mode);


void _name_init(void);


#endif
