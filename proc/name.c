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

#include "hal/hal.h"
#include "include/errno.h"
#include "lib/lib.h"
#include "proc.h"

#define HASH_LEN 5 /* Number of entries in dcache = 2 ^ HASH_LEN */


typedef struct _dcache_entry_t {
	struct _dcache_entry_t *next;
	oid_t oid;
	char name[];
} dcache_entry_t;


static struct {
	int root_registered;
	oid_t root_oid;

	dcache_entry_t *dcache[0x1U << HASH_LEN];
	lock_t dcache_lock;
} name_common;


/* Based on ceph_str_hash_linux() */
static unsigned int dcache_strHash(const char *str)
{
	unsigned int hash = 0;
	char c;

	c = *str;
	while (c != '\0') {
		hash += ((unsigned int)c << 4U) + ((unsigned int)c >> 4U) * 11U;
		str++;
		c = *str;
	}

	return hash & ((0x1U << HASH_LEN) - 1U);
}


static dcache_entry_t *_dcache_entryLookup(unsigned int hash, const char *name)
{
	dcache_entry_t *entry = name_common.dcache[hash];

	while (entry != NULL) {
		if (hal_strcmp(entry->name, name) == 0) {
			break;
		}

		entry = entry->next;
	}

	return entry;
}


int proc_portRegister(unsigned int port, const char *name, oid_t *oid)
{
	dcache_entry_t *entry;
	unsigned int hash = dcache_strHash(name);

	/* Check if entry already exists */
	(void)proc_lockSet(&name_common.dcache_lock);
	if (_dcache_entryLookup(hash, name) != NULL) {
		(void)proc_lockClear(&name_common.dcache_lock);
		return -EEXIST;
	}
	(void)proc_lockClear(&name_common.dcache_lock);

	if (name[0] == '/' && name[1] == '\0') {
		name_common.root_oid.port = port;
		if (oid != NULL) {
			name_common.root_oid.id = oid->id;
		}
		name_common.root_registered = 1;
		return EOK;
	}

	entry = vm_kmalloc(sizeof(dcache_entry_t) + hal_strlen(name) + 1U);
	if (entry == NULL) {
		return -ENOMEM;
	}

	entry->oid.port = port;
	if (oid != NULL) {
		entry->oid.id = oid->id;
	}

	(void)hal_strcpy(entry->name, name);

	(void)proc_lockSet(&name_common.dcache_lock);
	entry->next = name_common.dcache[hash];
	name_common.dcache[hash] = entry;
	(void)proc_lockClear(&name_common.dcache_lock);

	return EOK;
}


void proc_portUnregister(const char *name)
{
	dcache_entry_t *entry, *prev = NULL;
	unsigned int hash = dcache_strHash(name);

	(void)proc_lockSet(&name_common.dcache_lock);
	entry = name_common.dcache[hash];

	while (entry != NULL && hal_strcmp(entry->name, name) != 0) {
		/* Find entry to remove */
		prev = entry;
		entry = entry->next;
	}

	if (entry == NULL) {
		/* There is no such entry, nothing to do */
		(void)proc_lockClear(&name_common.dcache_lock);
		return;
	}

	if (prev != NULL) {
		prev->next = entry->next;
	}
	else {
		name_common.dcache[hash] = NULL;
	}
	(void)proc_lockClear(&name_common.dcache_lock);

	vm_kfree(entry);
}


int proc_portLookup(const char *name, oid_t *file, oid_t *dev)
{
	int err;
	dcache_entry_t *entry;
	msg_t *msg;
	size_t len, i;
	oid_t srv;
	char pstack[16], *pheap = NULL, *pptr;

	if (name == NULL || (file == NULL && dev == NULL)) {
		return -EINVAL;
	}

	if (name[0] == '/' && name[1] == '\0') {
		if (name_common.root_registered != 0) {
			if (file != NULL) {
				*file = name_common.root_oid;
			}

			if (dev != NULL) {
				*dev = name_common.root_oid;
			}
			return EOK;
		}

		return -EINVAL;
	}

	/* Search cache for full path */
	(void)proc_lockSet(&name_common.dcache_lock);
	entry = _dcache_entryLookup(dcache_strHash(name), name);
	if (entry != NULL) {
		if (file != NULL) {
			*file = entry->oid;
		}

		if (dev != NULL) {
			*dev = entry->oid;
		}
		(void)proc_lockClear(&name_common.dcache_lock);
		return EOK;
	}
	(void)proc_lockClear(&name_common.dcache_lock);

	srv = name_common.root_oid;

	/* Search cache for starting point */
	len = hal_strlen(name);

	if (len < sizeof(pstack)) {
		pptr = pstack;
	}
	else {
		pheap = vm_kmalloc(len + 1U);
		if (pheap == NULL) {
			return -ENOMEM;
		}

		pptr = pheap;
	}

	i = len;
	(void)hal_strcpy(pptr, name);

	while (i > 1U) {
		while (i > 0U && pptr[i] != '/') {
			--i;
		}

		if (i == 0U) {
			break;
		}

		pptr[i] = '\0';

		(void)proc_lockSet(&name_common.dcache_lock);
		entry = _dcache_entryLookup(dcache_strHash(pptr), pptr);
		if (entry != NULL) {
			srv = entry->oid;
			(void)proc_lockClear(&name_common.dcache_lock);
			break;
		}
		(void)proc_lockClear(&name_common.dcache_lock);
	}

	if (name_common.root_registered == 0 && i == 0U) {
		if (pheap != NULL) {
			vm_kfree(pheap);
		}
		return -EINVAL;
	}

	msg = vm_kmalloc(sizeof(msg_t));
	if (msg == NULL) {
		if (pheap != NULL) {
			vm_kfree(pheap);
		}
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));
	msg->type = mtLookup;

	/* Query servers */
	do {
		hal_memcpy(&msg->oid, &srv, sizeof(srv));
		msg->i.size = len - i;
		hal_memcpy(pptr, name + i + 1, len - i);
		msg->i.data = pptr;

		err = proc_send(srv.port, msg);
		if (err < 0) {
			break;
		}

		srv = msg->o.lookup.dev;
		err = msg->o.err;
		if (err < 0) {
			break;
		}

		i += (size_t)err + 1U;
		if (i > len) {
			err = -EINVAL;
			break;
		}
	} while (i != len);

	if (file != NULL) {
		*file = msg->o.lookup.fil;
	}
	if (dev != NULL) {
		*dev = msg->o.lookup.dev;
	}

	vm_kfree(msg);
	if (pheap != NULL) {
		vm_kfree(pheap);
	}
	return err < 0 ? err : EOK;
}


int proc_lookup(const char *name, oid_t *file, oid_t *dev)
{
	if (file != NULL) {
		file->id = 0;
	}

	return proc_portLookup(name, file, dev);
}


int proc_open(oid_t oid, unsigned int mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtOpen;
	hal_memcpy(&msg->oid, &oid, sizeof(oid_t));
	msg->i.openclose.flags = mode;

	err = proc_send(oid.port, msg);
	if (err == 0) {
		err = msg->o.err;
	}

	vm_kfree(msg);
	return err;
}


int proc_close(oid_t oid, unsigned int mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtClose;
	hal_memcpy(&msg->oid, &oid, sizeof(oid_t));
	msg->i.openclose.flags = mode;

	err = proc_send(oid.port, msg);

	if (err == EOK) {
		err = msg->o.err;
	}

	vm_kfree(msg);

	return err;
}


int proc_create(u32 port, int type, unsigned int mode, oid_t dev, oid_t dir, char *name, oid_t *oid)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtCreate;
	msg->i.create.type = type;
	msg->i.create.mode = mode;
	hal_memcpy(&msg->i.create.dev, &dev, sizeof(dev));
	hal_memcpy(&msg->oid, &dir, sizeof(dir));
	msg->i.data = name;
	msg->i.size = name == NULL ? 0 : hal_strlen(name) + 1U;

	err = proc_send(port, msg);

	if (err == 0) {
		err = msg->o.err;
	}

	hal_memcpy(oid, &msg->o.create.oid, sizeof(oid_t));
	vm_kfree(msg);
	return err;
}


int proc_link(oid_t dir, oid_t oid, const char *name)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtLink;
	hal_memcpy(&msg->oid, &dir, sizeof(oid_t));
	hal_memcpy(&msg->i.ln.oid, &oid, sizeof(oid_t));

	msg->i.size = hal_strlen(name) + 1U;
	msg->i.data = (const void *)name;

	err = proc_send(dir.port, msg);

	if (err == 0) {
		err = msg->o.err;
	}

	vm_kfree(msg);
	return err;
}


int proc_unlink(oid_t dir, oid_t oid, const char *name)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtUnlink;
	hal_memcpy(&msg->oid, &dir, sizeof(oid_t));
	hal_memcpy(&msg->i.ln.oid, &oid, sizeof(oid_t));

	msg->i.size = hal_strlen(name) + 1U;
	msg->i.data = name;

	err = proc_send(dir.port, msg);

	if (err == 0) {
		err = msg->o.err;
	}

	vm_kfree(msg);
	return err;
}


int proc_read(oid_t oid, off_t offs, void *buf, size_t sz, unsigned int mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtRead;
	hal_memcpy(&msg->oid, &oid, sizeof(oid_t));
	msg->i.io.offs = offs;
	msg->i.io.len = 0;
	msg->i.io.mode = mode;

	msg->o.size = sz;
	msg->o.data = buf;

	err = proc_send(oid.port, msg);

	if (err >= 0) {
		err = msg->o.err;
	}

	vm_kfree(msg);
	return err;
}


int proc_write(oid_t oid, off_t offs, void *buf, size_t sz, unsigned int mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtWrite;
	hal_memcpy(&msg->oid, &oid, sizeof(oid_t));
	msg->i.io.offs = offs;
	msg->i.io.len = 0;
	msg->i.io.mode = mode;

	msg->i.size = sz;
	msg->i.data = buf;

	err = proc_send(oid.port, msg);

	if (err >= 0) {
		err = msg->o.err;
	}

	vm_kfree(msg);
	return err;
}


off_t proc_size(oid_t oid)
{
	off_t err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtGetAttr;
	hal_memcpy(&msg->oid, &oid, sizeof(oid_t));
	msg->i.attr.type = 3; /* atSize */
	err = proc_send(oid.port, msg);
	if (err == EOK) {
		err = msg->o.err;
	}
	if (err == EOK) {
		err = (off_t)msg->o.attr.val;
	}

	vm_kfree(msg);
	return err;
}


void _name_init(void)
{
	(void)proc_lockInit(&name_common.dcache_lock, &proc_lockAttrDefault, "name.common");

	hal_memset(name_common.dcache, 0, sizeof(name_common.dcache));
	name_common.root_registered = 0;
}
