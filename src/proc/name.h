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

#include HAL


typedef struct {
	unsigned int id;
	size_t pos;
	unsigned char buff[];
} __attribute__((packed)) fsdata_t;


typedef struct {
	unsigned int mode;
	char name[];
} __attribute__((packed)) fsopen_t;


typedef unsigned int fsclose_t;


typedef struct {
	unsigned int port;
	char name[];
} __attribute__((packed)) fsmount_t;


typedef struct {
	oid_t oid;
	size_t pos;
	char path[];
} __attribute__((packed)) fslookup_t;


typedef struct {
	unsigned int id;
	unsigned int cmd;
	unsigned long arg;
} __attribute__((packed)) fsfcntl_t;


extern int proc_portRegister(unsigned int port, const char *name, oid_t *oid);


extern void proc_portUnregister(const char *name);


extern int proc_portLookup(const char *name, oid_t *oid);


extern int proc_lookup(const char *name, oid_t *oid);


extern int proc_read(oid_t oid, size_t offs, void *buf, size_t sz, unsigned mode);


extern int proc_link(oid_t dir, oid_t oid, char *name);


extern int proc_create(int port, int type, int mode, oid_t dev, oid_t dir, char *name, oid_t *oid);


extern int proc_close(oid_t oid, unsigned mode);


extern int proc_open(oid_t oid, unsigned mode);


extern int proc_size(oid_t oid);


extern int proc_write(oid_t oid, size_t offs, void *buf, size_t sz, unsigned mode);


extern void _name_init(void);


#endif
