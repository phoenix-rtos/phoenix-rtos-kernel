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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/hal.h"
#include "include/errno.h"
#include "include/limits.h"
#include "lib/lib.h"
#include "lib/usermem.h"
#include "proc.h"

#define HASH_LEN 5 /* Number of entries in dcache = 2 ^ HASH_LEN */


typedef struct _dcache_entry_t {
	struct _dcache_entry_t *next;
	oid_t oid;
	partition_t *part;
	char name[];
} dcache_entry_t;


static struct {
	dcache_entry_t *dcache[0x1U << HASH_LEN];

	lock_t lock;
} name_common;


/* Based on ceph_str_hash_linux() */
static unsigned int dcache_strHash(const char *str, size_t len)
{
	unsigned int hash = 0;
	char c;
	size_t i = 0;

	c = *str;
	while (c != '\0' && i < len) {
		hash += ((unsigned int)c << 4U) + ((unsigned int)c >> 4U) * 11U;
		str++;
		c = *str;
		i++;
	}

	return hash & ((0x1U << HASH_LEN) - 1U);
}


static dcache_entry_t *_dcache_entryLookup(unsigned int hash, const char *name, size_t len)
{
	dcache_entry_t *entry = name_common.dcache[hash];
	partition_t *part = proc_current()->process->partition;

	while (entry != NULL) {
		if ((entry->part == part) &&
				(entry->name[len] == '\0' && hal_strncmp(entry->name, name, len) == 0)) {
			break;
		}

		entry = entry->next;
	}

	return entry;
}


int proc_portRegister(u32 port, const char *name, const oid_t *oid)
{
	dcache_entry_t *entry;
	unsigned int hash;
	int err;
	size_t len;

	err = usermem_strnlen(name, STR_MAX);
	if (err < 0) {
		return err;
	}
	len = (size_t)err;

	USERMEM_TRY(
			{
				hash = dcache_strHash(name, len);
			},
			{
				return -EFAULT;
			});

	/* Pre-allocate entry to avoid holding the lock during allocation */
	entry = vm_kmalloc(sizeof(dcache_entry_t) + len + 1U);

	(void)proc_lockSet(&name_common.lock);

	/* Check if entry already exists */
	err = EOK;
	USERMEM_TRY(
			{
				if (_dcache_entryLookup(hash, name, len) != NULL) {
					err = -EEXIST;
				}
			},
			{
				err = -EFAULT;
			});
	if (err != EOK) {
		(void)proc_lockClear(&name_common.lock);
		if (entry != NULL) {
			vm_kfree(entry);
		}
		return err;
	}

	if (entry == NULL) {
		(void)proc_lockClear(&name_common.lock);
		return -ENOMEM;
	}

	entry->part = proc_current()->process->partition;
	entry->oid.port = port;
	entry->oid.id = (oid != NULL) ? oid->id : 0U;

	err = usermem_memcpy(entry->name, name, len);
	if (err < 0) {
		(void)proc_lockClear(&name_common.lock);
		vm_kfree(entry);
		return err;
	}
	entry->name[len] = '\0';

	entry->next = name_common.dcache[hash];
	name_common.dcache[hash] = entry;

	(void)proc_lockClear(&name_common.lock);

	return EOK;
}


int proc_portUnregister(const char *name)
{
	dcache_entry_t *entry, *prev = NULL;
	unsigned int hash;
	partition_t *part = proc_current()->process->partition;
	int err;
	size_t len;

	err = usermem_strnlen(name, STR_MAX);
	if (err < 0) {
		return err;
	}
	len = (size_t)err;

	(void)proc_lockSet(&name_common.lock);

	USERMEM_TRY(
			{
				hash = dcache_strHash(name, len);

				entry = name_common.dcache[hash];

				while (entry != NULL && (entry->name[len] != '\0' || hal_strncmp(entry->name, name, len) != 0 || entry->part != part)) {
					/* Find entry to remove */
					prev = entry;
					entry = entry->next;
				}
			},
			{
				(void)proc_lockClear(&name_common.lock);
				return -EFAULT;
			});

	if (entry == NULL) {
		/* There is no such entry, nothing to do */
		(void)proc_lockClear(&name_common.lock);
		return -ENOENT;
	}

	if (prev != NULL) {
		prev->next = entry->next;
	}
	else {
		name_common.dcache[hash] = entry->next;
	}

	(void)proc_lockClear(&name_common.lock);

	vm_kfree(entry);

	return EOK;
}


int proc_portLookup(const char *name, oid_t *file, oid_t *dev)
{
	int err;
	dcache_entry_t *entry;
	unsigned int hash;
	msg_t *msg;
	size_t len, i, plen;
	oid_t srv;
	char pstack[16], *pheap = NULL, *pptr;

	if (name == NULL || (file == NULL && dev == NULL)) {
		return -EINVAL;
	}

	err = usermem_strnlen(name, STR_MAX);
	if (err < 0) {
		return err;
	}
	len = (size_t)err;

	USERMEM_TRY(
			{
				hash = dcache_strHash(name, len);
			},
			{
				return -EFAULT;
			});

	(void)proc_lockSet(&name_common.lock);

	/* Search cache for full path (fast path) */
	USERMEM_TRY(
			{
				entry = _dcache_entryLookup(hash, name, len);
			},
			{
				(void)proc_lockClear(&name_common.lock);
				return -EFAULT;
			});
	if (entry != NULL) {
		if (file != NULL) {
			*file = entry->oid;
		}

		if (dev != NULL) {
			*dev = entry->oid;
		}

		(void)proc_lockClear(&name_common.lock);
		return EOK;
	}

	/* Avoid holding the lock during allocation */
	(void)proc_lockClear(&name_common.lock);

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

	(void)proc_lockSet(&name_common.lock);

	i = len;
	err = usermem_memcpy(pptr, name, len);
	if (err < 0) {
		(void)proc_lockClear(&name_common.lock);
		if (pheap != NULL) {
			vm_kfree(pheap);
		}
		return err;
	}
	pptr[i] = '\0';

	do {
		while (i > 0U && pptr[i] != '/') {
			--i;
		}

		if (i != 0U) {
			pptr[i] = '\0';
			plen = i;
		}
		else if (pptr[0] == '/') {
			pptr[1] = '\0';
			plen = 1U;
		}
		else {
			break;
		}

		entry = _dcache_entryLookup(dcache_strHash(pptr, plen), pptr, plen);
		if (entry != NULL) {
			srv = entry->oid;
			break;
		}
	} while (i > 0U);

	(void)proc_lockClear(&name_common.lock);

	if (entry == NULL) {
		if (pheap != NULL) {
			vm_kfree(pheap);
		}
		return -ENOENT;
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
		err = usermem_memcpy(pptr, name + i + 1, len - i);
		if (err < 0) {
			break;
		}
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


int proc_destroy(u32 port, oid_t dev)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL) {
		return -ENOMEM;
	}

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtDestroy;
	hal_memcpy(&msg->oid, &dev, sizeof(dev));

	err = proc_send(port, msg);

	if (err == 0) {
		err = msg->o.err;
	}

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

	err = usermem_strnlen(name, STR_MAX);
	if (err < 0) {
		vm_kfree(msg);
		return err;
	}
	msg->i.size = (size_t)err + 1U;
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

	err = usermem_strnlen(name, STR_MAX);
	if (err < 0) {
		vm_kfree(msg);
		return err;
	}
	msg->i.size = (size_t)err + 1U;
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
	(void)proc_lockInit(&name_common.lock, &proc_lockAttrDefault, "name.common");

	hal_memset(name_common.dcache, 0, sizeof(name_common.dcache));
}
