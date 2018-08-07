/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process resources
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "mutex.h"
#include "threads.h"
#include "file.h"
#include "cond.h"
#include "resource.h"
#include "name.h"
#include "userintr.h"


static int resource_cmp(rbnode_t *n1, rbnode_t *n2)
{
	resource_t *r1 = lib_treeof(resource_t, linkage, n1);
	resource_t *r2 = lib_treeof(resource_t, linkage, n2);

	return (r1->id - r2->id);
}


static int resource_gapcmp(rbnode_t *n1, rbnode_t *n2)
{
	resource_t *r1 = lib_treeof(resource_t, linkage, n1);
	resource_t *r2 = lib_treeof(resource_t, linkage, n2);
	rbnode_t *child = NULL;
	int ret = 1;

	if (r1->lmaxgap > 0 && r1->rmaxgap > 0) {
		if (r2->id > r1->id) {
			child = n1->right;
			ret = -1;
		}
		else {
			child = n1->left;
			ret = 1;
		}
	}
	else if (r1->lmaxgap > 0) {
		child = n1->left;
		ret = 1;
	}
	else if (r1->rmaxgap > 0) {
		child = n1->right;
		ret = -1;
	}

	if (child == NULL)
		return 0;

	return ret;
}


static void resource_augment(rbnode_t *node)
{
	rbnode_t *it;
	resource_t *n = lib_treeof(resource_t, linkage, node);
	resource_t *p = n, *r, *l;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(resource_t, linkage, it->parent);
			if (it->parent->right == it)
				break;
		}

		n->lmaxgap = (n->id <= p->id) ? n->id : n->id - p->id - 1;
	}
	else {
		l = lib_treeof(resource_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(resource_t, linkage, it->parent);
			if (it->parent->left == it)
				break;
		}

		n->rmaxgap = (n->id >= p->id) ? (unsigned)-1 - n->id - 1 : p->id - n->id - 1;
	}
	else {
		r = lib_treeof(resource_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(resource_t, linkage, it);
		p = lib_treeof(resource_t, linkage, it->parent);

		if (it->parent->left == it)
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		else
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
	}
}


resource_t *resource_alloc(process_t *process, unsigned int *id, int type)
{
	resource_t *r, t;

	proc_lockSet(process->rlock);

	if (process->resources->root != NULL) {
		t.id = 0;
		r = lib_treeof(resource_t, linkage, lib_rbFindEx(process->resources->root, &t.linkage, resource_gapcmp));
		if (r != NULL) {
			if (r->lmaxgap > 0)
				*id = r->id - 1;
			else
				*id = r->id + 1;
		}
		else {
			proc_lockClear(process->rlock);
			return NULL;
		}
	}

	if ((r = (resource_t *)vm_kmalloc(sizeof(resource_t))) == NULL) {
		proc_lockClear(process->rlock);
		return NULL;
	}

	switch (type) {
	case rtLock:
		if ((r->lock = vm_kmalloc(sizeof(lock_t))) == NULL) {
			vm_kfree(r);
			proc_lockClear(process->rlock);
			return NULL;
		}
		break;

	case rtCond:
		r->waitq = NULL;
		break;

	case rtFile:
		if ((r->fd = vm_kmalloc(sizeof(fd_t))) == NULL) {
			vm_kfree(r);
			proc_lockClear(process->rlock);
			return NULL;
		}
		break;

	case rtInth:
		if ((r->inth = vm_kmalloc(sizeof(intr_handler_t))) == NULL) {
			vm_kfree(r);
			proc_lockClear(process->rlock);
			return NULL;
		}
		break;

	default:
		vm_kfree(r);
		proc_lockClear(process->rlock);
		return NULL;
	}

	r->id = *id;
	r->refs = 1;
	r->type = type;

	lib_rbInsert(process->resources, &r->linkage);
	proc_lockClear(process->rlock);

	return r;
}


int proc_resourcesCopy(process_t *src)
{
	rbnode_t *n;
	resource_t *r, *d = NULL;
	process_t *process;
	int err = EOK;

	process = proc_current()->process;

	proc_lockSet(&src->lock);
	for (n = lib_rbMinimum(src->resources->root); n != NULL; n = lib_rbNext(n)) {
		r = lib_treeof(resource_t, linkage, n);

		if (d == NULL && (d = vm_kmalloc(sizeof(resource_t))) == NULL) {
			err = -ENOMEM;
			break;
		}

		d->id = r->id;
		d->refs = 0;

		switch (r->type) {
		case rtLock:
			if ((d->lock = vm_kmalloc(sizeof(lock_t))) == NULL)
				err = -ENOMEM;
			else
				err = proc_mutexCopy(d, r);
			break;
		case rtCond:
			err = proc_condCopy(d, r);
			break;
		case rtFile:
			if ((d->fd = vm_kmalloc(sizeof(fd_t))) == NULL)
				err = -ENOMEM;
			else if (!(err = proc_fileCopy(d, r)))
				proc_open(d->fd->oid, d->fd->mode);
			break;
		case rtInth:
			err = 1; /* Don't copy interrupt handlers */
			d->inth = NULL;
			break;
		}

		if (err == EOK) {
			lib_rbInsert(process->resources, &d->linkage);
			d = NULL;
		}
		else if (err > 0) {
			err = EOK;
			continue;
		}
		else {
			break;
		}
	}
	proc_lockClear(&src->lock);

	if (d != NULL) {
		if (d->type == rtLock && d->lock != NULL)
			vm_kfree(d->lock);
		else if (d->type == rtFile && d->fd != NULL)
			vm_kfree(d->fd);
		else if (d->type == rtInth && d->inth != NULL)
			vm_kfree(d->inth);

		vm_kfree(d);
	}

	return err;
}


int resource_free(resource_t *r)
{
	process_t *process;

	process = proc_current()->process;

	proc_lockSet(process->rlock);

	if (r->refs > 1) {
		proc_lockClear(process->rlock);
		return -EBUSY;
	}

	switch (r->type) {
	case rtLock:
		proc_lockDone(r->lock);
		vm_kfree(r->lock);
		break;
	case rtFile:
		/* proc_close? */
		vm_kfree(r->fd);
		break;
	case rtInth:
		hal_interruptsDeleteHandler(r->inth);
		vm_kfree(r->inth);
		break;
	default:
		break;
	}

	lib_rbRemove(process->resources, &r->linkage);
	proc_lockClear(process->rlock);

	vm_kfree(r);

	return EOK;
}


void proc_resourcesFree(process_t *proc)
{
	rbnode_t *n;
	resource_t *r;

	/* Don't free if they share parent's resources */
	if (proc->resources != &proc->resourcetree)
		return;

	proc_lockSet(proc->rlock);
	for (n = proc->resources->root; n != NULL;
	     proc_lockSet(proc->rlock), n = proc->resources->root) {
		lib_rbRemove(proc->resources, n);
		r = lib_treeof(resource_t, linkage, n);
		proc_lockClear(proc->rlock);

		switch (r->type) {
		case rtFile:
			proc_close(r->fd->oid, r->fd->mode);
			vm_kfree(r->fd);
			break;
		case rtLock:
			proc_lockDone(r->lock);
			vm_kfree(r->lock);
			break;
		case rtInth:
			hal_interruptsDeleteHandler(r->inth);
			vm_kfree(r->inth);
			break;
		default:
			break;
		}

		vm_kfree(r);
	}
	proc_lockClear(proc->rlock);
}


resource_t *resource_get(process_t *process, unsigned int id)
{
	resource_t *r, t;

	t.id = id;

	proc_lockSet(process->rlock);
	if ((r = lib_treeof(resource_t, linkage, lib_rbFind(process->resources, &t.linkage))) != NULL)
		r->refs++;
	proc_lockClear(process->rlock);

	return r;
}


void resource_put(process_t *process, resource_t *r)
{
	proc_lockSet(process->rlock);
	if (r->refs)
		r->refs--;
	proc_lockClear(process->rlock);
	return;
}


int proc_resourceFree(unsigned int h)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_get(process, h)) == NULL)
		return -EINVAL;

	return resource_free(r);
}


void resource_init(process_t *process)
{
	lib_rbInit(&process->resourcetree, resource_cmp, resource_augment);
	process->resources = &process->resourcetree;
	process->rlock = &process->lock;
}
