/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Names resolving
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../lib/lib.h"
#include "proc.h"

#define HASH_LEN 5 /* Number of entries in dcache = 2 ^ HASH_LEN */


typedef struct _dcache_entry_t {
	struct _dcache_entry_t *next;
	oid_t oid;
	char name[];
} dcache_entry_t;


struct {
	int root_registered;
	oid_t root_oid;

	dcache_entry_t *dcache[1 << HASH_LEN];
	lock_t dcache_lock;
} name_common;


/* Based on ceph_str_hash_linux() */
static unsigned int dcache_strHash(const char *str)
{
	unsigned int hash = 0;
	unsigned char c;

	while ((c = *str++) != '\0')
		hash += (c << 4) + (c >> 4) * 11;

	return hash & ((1 << HASH_LEN) - 1);
}


static dcache_entry_t *_dcache_entryLookup(unsigned int hash, const char *name)
{
	dcache_entry_t *entry = name_common.dcache[hash];

	while (entry != NULL && hal_strcmp(entry->name, name) != 0)
		entry = entry->next;

	return entry;
}


int proc_portRegister(unsigned int port, const char *name, oid_t *oid)
{
	dcache_entry_t *entry;
	unsigned int hash = dcache_strHash(name);

	/* Check if entry already exists */
	proc_lockSet(&name_common.dcache_lock);
	if (_dcache_entryLookup(hash, name) != NULL) {
		proc_lockClear(&name_common.dcache_lock);
		return -EEXIST;
	}
	proc_lockClear(&name_common.dcache_lock);

	if ((entry = vm_kmalloc(sizeof(dcache_entry_t) + hal_strlen(name) + 1)) == NULL)
		return -ENOMEM;

	entry->oid.port = port;
	if (oid != NULL)
		entry->oid.id = oid->id;
	hal_strcpy(entry->name, name);

	if (name[0] == '/' && name[1] == 0) {
		name_common.root_oid = entry->oid;
		name_common.root_registered = 1;
		return EOK;
	}

	proc_lockSet(&name_common.dcache_lock);
	entry->next = name_common.dcache[hash];
	name_common.dcache[hash] = entry;
	proc_lockClear(&name_common.dcache_lock);

	return EOK;
}


void proc_portUnregister(const char *name)
{
	dcache_entry_t *entry, *prev = NULL;
	unsigned int hash = dcache_strHash(name);

	proc_lockSet(&name_common.dcache_lock);
	entry = name_common.dcache[hash];

	while (entry != NULL && hal_strcmp(entry->name, name) != 0) {
		/* Find entry to remove */
		prev = entry;
		entry = entry->next;
	}

	if (entry == NULL) {
		/* There is no such entry, nothing to do */
		proc_lockClear(&name_common.dcache_lock);
		return;
	}

	if (prev != NULL)
		prev->next = entry->next;
	else
		name_common.dcache[hash] = NULL;
	proc_lockClear(&name_common.dcache_lock);

	vm_kfree(entry);
}


int proc_portLookup(const char *name, oid_t *oid)
{
	int err;
	dcache_entry_t *entry;
	kmsg_t *kmsg;
	size_t len, i;
	oid_t srv;
	char pstack[16], *pheap = NULL, *pptr;

	if (name == NULL || oid == NULL || name[0] != '/')
		return -EINVAL;

	if (name[1] == 0) {
		if (name_common.root_registered) {
			*oid = name_common.root_oid;
			return EOK;
		}

		return -EINVAL;
	}

	/* Search cache for full path */
	proc_lockSet(&name_common.dcache_lock);
	if ((entry = _dcache_entryLookup(dcache_strHash(name), name)) != NULL) {
		*oid = entry->oid;
		proc_lockClear(&name_common.dcache_lock);
		return EOK;
	}
	proc_lockClear(&name_common.dcache_lock);

	srv = name_common.root_oid;

#if 1 /* (MOD) */
	/* Search cache for starting point */
	len = hal_strlen(name);

	if (len < sizeof(pstack)) {
		pptr = pstack;
	}
	else {
		if ((pheap = vm_kmalloc(len + 1)) == NULL)
			return -ENOMEM;
		pptr = pheap;
	}

	i = len;
	hal_strcpy(pptr, name);

	while (i > 1) {
		while (pptr[--i] != '/');

		if (i == 0)
			break;

		pptr[i] = '\0';

		proc_lockSet(&name_common.dcache_lock);
		if ((entry = _dcache_entryLookup(dcache_strHash(pptr), pptr)) != NULL) {
			srv = entry->oid;
			proc_lockClear(&name_common.dcache_lock);
			break;
		}
		proc_lockClear(&name_common.dcache_lock);
	}

	if (!name_common.root_registered && !i)
		return -EINVAL;

	if ((kmsg = vm_kmalloc(sizeof(kmsg_t))) == NULL)
		return -ENOMEM;

	kmsg->threads = NULL;
	kmsg->responded = 0;

	hal_memset(&kmsg->msg, 0, sizeof(msg_t));
	kmsg->msg.type = mtLookup;

	/* Query servers */
	do {
		kmsg->msg.i.lookup.dir = srv;
		kmsg->msg.i.size = len - i;
		hal_memcpy(pptr, name + i + 1, len - i);
		kmsg->msg.i.data = pptr;

		if ((err = proc_send(srv.port, kmsg)) < 0)
			break;

		srv = kmsg->msg.o.lookup.res;

		if ((err = kmsg->msg.o.lookup.err) < 0)
			break;

		if (i + err > len) {
			err = -EINVAL;
			break;
		}

		i += err + 1;
	}
	while (i != len);

	*oid = kmsg->msg.o.lookup.res;

	vm_kfree(kmsg);
	return err < 0 ? err : EOK;
#endif

}


int proc_lookup(const char *name, oid_t *oid)
{
	oid->id = 0;

	return proc_portLookup((char *)name, oid);
}


int proc_open(oid_t oid)
{
	int err;
	kmsg_t *kmsg = vm_kmalloc(sizeof(kmsg_t));

	if (kmsg == NULL)
		return -ENOMEM;

	hal_memset(kmsg, 0, sizeof(kmsg_t));

	kmsg->threads = NULL;
	kmsg->responded = 0;

	kmsg->msg.type = mtOpen;
	hal_memcpy(&kmsg->msg.i.openclose.oid, &oid, sizeof(oid_t));
	kmsg->msg.i.openclose.flags = 0;

	err = proc_send(oid.port, kmsg);
	vm_kfree(kmsg);
	return err;
}


int proc_close(oid_t oid)
{
	int err;
	kmsg_t *kmsg = vm_kmalloc(sizeof(kmsg_t));

	if (kmsg == NULL)
		return -ENOMEM;

	hal_memset(kmsg, 0, sizeof(kmsg_t));

	kmsg->threads = NULL;
	kmsg->responded = 0;

	kmsg->msg.type = mtClose;
	hal_memcpy(&kmsg->msg.i.openclose.oid, &oid, sizeof(oid_t));
	kmsg->msg.i.openclose.flags = 0;

	err = proc_send(oid.port, kmsg);
	vm_kfree(kmsg);
	return err;
}


int proc_create(int port, oid_t *oid, int type)
{
	int err;
	kmsg_t *kmsg = vm_kmalloc(sizeof(kmsg_t));

	if (kmsg == NULL)
		return -ENOMEM;

	hal_memset(kmsg, 0, sizeof(kmsg_t));

	kmsg->threads = NULL;
	kmsg->responded = 0;

	kmsg->msg.type = mtCreate;
	kmsg->msg.i.create.type = type;
	kmsg->msg.i.create.mode = 0;
	kmsg->msg.i.create.port = 0;

	err = proc_send(port, kmsg);

	hal_memcpy(oid, &kmsg->msg.o.create.oid, sizeof(oid_t));
	vm_kfree(kmsg);
	return err;
}


int proc_link(oid_t dir, oid_t oid, char *name)
{
	int err;
	kmsg_t *kmsg = vm_kmalloc(sizeof(kmsg_t));

	if (kmsg == NULL)
		return -ENOMEM;

	hal_memset(kmsg, 0, sizeof(kmsg_t));

	kmsg->threads = NULL;
	kmsg->responded = 0;

	kmsg->msg.type = mtLink;
	hal_memcpy(&kmsg->msg.i.ln.dir, &dir, sizeof(oid_t));
	hal_memcpy(&kmsg->msg.i.ln.oid, &oid, sizeof(oid_t));

	kmsg->msg.i.size = hal_strlen(name) + 1;
	kmsg->msg.i.data = name;

	err = proc_send(dir.port, kmsg);
	vm_kfree(kmsg);
	return err;
}


int proc_read(oid_t oid, size_t offs, void *buf, size_t sz)
{
	int err;
	kmsg_t *kmsg = vm_kmalloc(sizeof(kmsg_t));

	if (kmsg == NULL)
		return -ENOMEM;

	hal_memset(kmsg, 0, sizeof(kmsg_t));

	kmsg->threads = NULL;
	kmsg->responded = 0;

	kmsg->msg.type = mtRead;
	hal_memcpy(&kmsg->msg.i.io.oid, &oid, sizeof(oid_t));
	kmsg->msg.i.io.offs = offs;
	kmsg->msg.i.io.len = 0;

	kmsg->msg.o.size = sz;
	kmsg->msg.o.data = buf;

	err = proc_send(oid.port, kmsg);

	if (err >= 0)
		err = kmsg->msg.o.io.err;

	vm_kfree(kmsg);
	return err;
}


int proc_write(oid_t oid, size_t offs, void *buf, size_t sz)
{
	int err;
	kmsg_t *kmsg = vm_kmalloc(sizeof(kmsg_t));

	if (kmsg == NULL)
		return -ENOMEM;

	hal_memset(kmsg, 0, sizeof(kmsg_t));

	kmsg->threads = NULL;
	kmsg->responded = 0;

	kmsg->msg.type = mtWrite;
	hal_memcpy(&kmsg->msg.i.io.oid, &oid, sizeof(oid_t));
	kmsg->msg.i.io.offs = offs;
	kmsg->msg.i.io.len = 0;

	kmsg->msg.i.size = sz;
	kmsg->msg.i.data = buf;

	err = proc_send(oid.port, kmsg);

	if (err >= 0)
		err = kmsg->msg.o.io.err;

	vm_kfree(kmsg);
	return err;
}


int proc_size(oid_t oid)
{
	int err;
	kmsg_t *kmsg = vm_kmalloc(sizeof(kmsg_t));

	if (kmsg == NULL)
		return -ENOMEM;

	hal_memset(kmsg, 0, sizeof(kmsg_t));

	kmsg->threads = NULL;
	kmsg->responded = 0;

	kmsg->msg.type = mtGetAttr;
	hal_memcpy(&kmsg->msg.i.attr.oid, &oid, sizeof(oid_t));
	kmsg->msg.i.attr.type = 3; /* atSize */

	err = proc_send(oid.port, kmsg);

	if (err >= 0)
		err = kmsg->msg.o.attr.val;

	vm_kfree(kmsg);
	return err;
}


void _name_init(void)
{
	proc_lockInit(&name_common.dcache_lock);

	hal_memset(name_common.dcache, NULL, sizeof(name_common.dcache));
	name_common.root_registered = 0;
}
