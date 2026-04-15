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

/* TODO: TEMPORARY */
#include "log/log.h"

#define HASH_LEN 5 /* Number of entries in dcache = 2 ^ HASH_LEN */


#define WARN_ON_OLD_API LIB_ASSERT(0, "!!!!! %s", __FUNCTION__)

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

	if (name[0] == '/' && name[1] == 0) {
		name_common.root_oid.port = port;
		if (oid != NULL)
			name_common.root_oid.id = oid->id;
		name_common.root_registered = 1;
		return EOK;
	}

	if ((entry = vm_kmalloc(sizeof(dcache_entry_t) + hal_strlen(name) + 1)) == NULL)
		return -ENOMEM;

	entry->oid.port = port;
	if (oid != NULL)
		entry->oid.id = oid->id;
	hal_strcpy(entry->name, name);

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


int proc_portLookup(const char *name, oid_t *file, oid_t *dev)
{
	int err;
	dcache_entry_t *entry;
	size_t len, i;
	oid_t srv;
	char pstack[16], *pheap = NULL, *pptr;
	msgHeader_t hdr;
	char odata[64];

	msg_lookup_rsp_t *lookupOut = (msg_lookup_rsp_t *)odata;

	if (name == NULL || (file == NULL && dev == NULL))
		return -EINVAL;

	if (name[0] == '/' && name[1] == 0) {
		if (name_common.root_registered) {
			if (file != NULL)
				*file = name_common.root_oid;
			if (dev != NULL)
				*dev = name_common.root_oid;
			return EOK;
		}

		return -EINVAL;
	}

	/* Search cache for full path */
	proc_lockSet(&name_common.dcache_lock);
	if ((entry = _dcache_entryLookup(dcache_strHash(name), name)) != NULL) {
		if (file != NULL)
			*file = entry->oid;
		if (dev != NULL)
			*dev = entry->oid;
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
		while (i > 0 && pptr[i] != '/') {
			--i;
		}

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

	if (!name_common.root_registered && !i) {
		if (pheap != NULL)
			vm_kfree(pheap);
		return -EINVAL;
	}

	/* Query servers */
	do {
		hal_memset(&hdr, 0, sizeof(hdr));
		hdr.type = mtLookup;
		hdr.oid = srv;

		hal_memcpy(pptr, name + i + 1, len - i);

		err = proc_call_returnable(srv.port, &hdr, pptr, len - i, odata, sizeof(odata));
		if (err < 0) {
			break;
		}

		srv = lookupOut->dev;
		err = hdr.err;
		if (err < 0) {
			break;
		}

		i += err + 1;
		if (i > len) {
			err = -EINVAL;
			break;
		}
	} while (i != len);

	if (file != NULL) {
		*file = lookupOut->fil;
	}
	if (dev != NULL) {
		*dev = lookupOut->dev;
	}

	if (pheap != NULL) {
		vm_kfree(pheap);
	}

	return err < 0 ? err : EOK;
#endif
}


int proc_lookup(const char *name, oid_t *file, oid_t *dev)
{
	if (file != NULL)
		file->id = 0;

	return proc_portLookup((char *)name, file, dev);
}


int proc_create(int port, int type, int mode, oid_t dev, oid_t dir, char *name, oid_t *oid)
{
	int err;
	msgHeader_t hdr;
	char odata[64];

	msg_create_t createIn;
	msg_create_rsp_t *createOut = (msg_create_rsp_t *)odata;

	createIn.type = type;
	createIn.mode = mode;
	hal_memcpy(&createIn.dev, &dev, sizeof(dev));

	hal_memset(&hdr, 0, sizeof(hdr));
	hdr.type = mtCreate;
	hal_memcpy(&hdr.oid, &dir, sizeof(dir));
	hdr.iextra = &createIn;
	hdr.iesize = sizeof(createIn);

	err = proc_call_returnable(port, &hdr, name, name == NULL ? 0 : hal_strlen(name) + 1, odata, sizeof(odata));

	if (err == 0) {
		err = hdr.err;
	}

	hal_memcpy(oid, &createOut->oid, sizeof(oid_t));
	return err;
}


int proc_link(oid_t dir, oid_t oid, const char *name)
{
	WARN_ON_OLD_API;
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL)
		return -ENOMEM;

	hal_memset(msg, 0, sizeof(msg_t));

	msg->type = mtLink;
	hal_memcpy(&msg->oid, &dir, sizeof(oid_t));
	hal_memcpy(&msg->i.ln.oid, &oid, sizeof(oid_t));

	msg->i.size = hal_strlen(name) + 1;
	msg->i.data = (char *)name;

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
	msgHeader_t hdr;
	char odata[64];

	msg_link_t unlinkIn;

	hal_memcpy(&unlinkIn.oid, &oid, sizeof(oid_t));

	hal_memset(&hdr, 0, sizeof(hdr));
	hdr.type = mtUnlink;
	hal_memcpy(&hdr.oid, &dir, sizeof(oid_t));
	hdr.iextra = &unlinkIn;
	hdr.iesize = sizeof(unlinkIn);

	err = proc_call_returnable(dir.port, &hdr, (void *)name, hal_strlen(name) + 1, odata, sizeof(odata));

	if (err == 0) {
		err = hdr.err;
	}

	return err;
}

#define NEW_API 1


#if !NEW_API
int proc_read(oid_t oid, off_t offs, void *buf, size_t sz, unsigned mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL)
		return -ENOMEM;

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


int proc_write(oid_t oid, off_t offs, void *buf, size_t sz, unsigned mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL)
		return -ENOMEM;

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


int proc_open(oid_t oid, unsigned mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL)
		return -ENOMEM;

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


int proc_close(oid_t oid, unsigned mode)
{
	int err;
	msg_t *msg = vm_kmalloc(sizeof(msg_t));

	if (msg == NULL)
		return -ENOMEM;

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


#else

int proc_read(oid_t oid, off_t offs, void *buf, size_t sz, unsigned mode)
{
	lib_debug_printf("%s (%zu, %p, %zu, %d)\n", __FUNCTION__, offs, buf, sz, mode);

	int err;
	msgHeader_t hdr;
	msg_io_t ioIn;

	ioIn.offs = offs;
	ioIn.len = 0;
	ioIn.mode = mode;

	hal_memset(&hdr, 0, sizeof(hdr));
	hdr.type = mtRead;
	hal_memcpy(&hdr.oid, &oid, sizeof(oid_t));
	hdr.iextra = &ioIn;
	hdr.iesize = sizeof(ioIn);

	/*
	 * TODO: add some proc_call variant that automatically sets rv to hdr.err on msgRespond/msgRespondAndRecv side
	 * the proc_call_returnable is a slower variant and should be avoided
	 */
	err = proc_call_returnable(oid.port, &hdr, NULL, 0, buf, sz);
	return err == EOK ? hdr.err : err;
}


int proc_write(oid_t oid, off_t offs, void *buf, size_t sz, unsigned mode)
{
	lib_debug_printf("%s (buf=%p)\n", __FUNCTION__, buf);

	int err;
	msgHeader_t hdr;
	char odata[64];
	msg_io_t ioIn;

	ioIn.offs = offs;
	ioIn.len = 0;
	ioIn.mode = mode;

	hal_memset(&hdr, 0, sizeof(hdr));
	hdr.type = mtWrite;
	hal_memcpy(&hdr.oid, &oid, sizeof(oid_t));
	hdr.iextra = &ioIn;
	hdr.iesize = sizeof(ioIn);

	err = proc_call_returnable(oid.port, &hdr, buf, sz, odata, sizeof(odata));
	return err == EOK ? hdr.err : err;
}

int proc_open(oid_t oid, unsigned mode)
{
	lib_debug_printf("%s\n", __FUNCTION__);

	int err;
	msgHeader_t hdr;
	char odata[64];
	msg_open_t openIn;

	openIn.flags = mode;

	hal_memset(&hdr, 0, sizeof(hdr));
	hdr.type = mtOpen;
	hal_memcpy(&hdr.oid, &oid, sizeof(oid_t));
	hdr.iextra = &openIn;
	hdr.iesize = sizeof(openIn);

	err = proc_call_returnable(oid.port, &hdr, NULL, 0, odata, sizeof(odata));
	return err == EOK ? hdr.err : err;
}


int proc_close(oid_t oid, unsigned mode)
{
	lib_debug_printf("%s\n", __FUNCTION__);

	int err;
	msgHeader_t hdr;
	char odata[64];
	msg_open_t closeIn;

	closeIn.flags = mode;

	hal_memset(&hdr, 0, sizeof(hdr));
	hdr.type = mtClose;
	hal_memcpy(&hdr.oid, &oid, sizeof(oid_t));
	hdr.iextra = &closeIn;
	hdr.iesize = sizeof(closeIn);

	err = proc_call_returnable(oid.port, &hdr, NULL, 0, odata, sizeof(odata));
	return err == EOK ? hdr.err : err;
}

#endif


off_t proc_size(oid_t oid)
{
	int err;
	msgHeader_t hdr;
	char odata[64];
	msg_attr_t attrIn;
	msg_attr_rsp_t *attrOut = (msg_attr_rsp_t *)odata;

	attrIn.type = 3; /* atSize */
	attrIn.val = 0;

	hal_memset(&hdr, 0, sizeof(hdr));
	hdr.type = mtGetAttr;
	hal_memcpy(&hdr.oid, &oid, sizeof(oid_t));
	hdr.iextra = &attrIn;
	hdr.iesize = sizeof(attrIn);

	err = proc_call_returnable(oid.port, &hdr, NULL, 0, odata, sizeof(odata));
	if (err == EOK) {
		err = hdr.err;
	}
	if (err == EOK) {
		err = attrOut->val;
	}

	return err;
}


void _name_init(void)
{
	proc_lockInit(&name_common.dcache_lock, &proc_lockAttrDefault, "name.common");

	hal_memset(name_common.dcache, NULL, sizeof(name_common.dcache));
	name_common.root_registered = 0;
}
