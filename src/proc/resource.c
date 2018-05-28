/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process resources
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
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
#include "resource.h"
#include "name.h"


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
	rbnode_t *parent = node->parent;
	resource_t *n = lib_treeof(resource_t, linkage, node);
	resource_t *p = lib_treeof(resource_t, linkage, parent);
	resource_t *pp = (parent != NULL) ? lib_treeof(resource_t, linkage, parent->parent) : NULL;
	resource_t *l, *r;

	if (node->left == NULL) {
		if (parent != NULL && parent->right == node)
			n->lmaxgap = n->id - p->id - 1;
		else if (parent != NULL && parent->parent != NULL && parent->parent->right == parent)
			n->lmaxgap = n->id - pp->id - 1;
		else
			n->lmaxgap = n->id;
	}
	else {
		l = lib_treeof(resource_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		if (parent != NULL && parent->left == node)
			n->rmaxgap = p->id - n->id - 1;
		else if (parent != NULL && parent->parent != NULL && parent->parent->left == parent)
			n->rmaxgap = pp->id - n->id - 1;
		else
			n->rmaxgap = (unsigned int)-1 - n->id - 1;
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


resource_t *resource_alloc(process_t *process, unsigned int *id)
{
	resource_t *r, t;

	proc_lockSet(&process->lock);

	if (process->resources.root != NULL) {
		t.id = *id;
		r = lib_treeof(resource_t, linkage, lib_rbFindEx(process->resources.root, &t.linkage, resource_gapcmp));
		if (r != NULL) {
			if (!(r->id < *id && (r->rmaxgap >= *id - r->id)) && !(r->id > *id && (r->lmaxgap >= r->id - *id))) {
				if (r->lmaxgap > 0)
					*id = r->id - 1;
				else
					*id = r->id + 1;
			}
		}
		else {
			proc_lockClear(&process->lock);
			return NULL;
		}
	}

	if ((r = (resource_t *)vm_kmalloc(sizeof(resource_t))) == NULL) {
		proc_lockClear(&process->lock);
		return NULL;
	}

	r->id = *id;
	r->refs = 1;

	lib_rbInsert(&process->resources, &r->linkage);
	proc_lockClear(&process->lock);

	return r;
}


int resource_free(resource_t *r)
{
	process_t *process;

	process = proc_current()->process;

	proc_lockSet(&process->lock);

	if (r->refs > 1) {
		proc_lockClear(&process->lock);
		return -EBUSY;
	}

	switch (r->type) {
	case rtLock:
		proc_lockDone(&r->lock);
		break;
	case rtInth:
		hal_interruptsDeleteHandler(&r->inth);
		break;
	default:
		break;
	}

	lib_rbRemove(&process->resources, &r->linkage);
	proc_lockClear(&process->lock);

	vm_kfree(r);

	return EOK;
}


void proc_resourcesFree(process_t *proc)
{
	rbnode_t *n;
	resource_t *r;

	proc_lockSet(&proc->lock);
	for (n = proc->resources.root; n != NULL;
	     proc_lockSet(&proc->lock), n = proc->resources.root) {
		lib_rbRemove(&proc->resources, n);
		r = lib_treeof(resource_t, linkage, n);
		proc_lockClear(&proc->lock);

		switch (r->type) {
		case rtFile:
			proc_close(r->oid);
			break;
		case rtLock:
			proc_lockDone(&r->lock);
			break;
		case rtInth:
			hal_interruptsDeleteHandler(&r->inth);
			break;
		default:
			break;
		}

		vm_kfree(r);
	}
	proc_lockClear(&proc->lock);
}


int proc_resourcesCopy(process_t *src)
{
	rbnode_t *n;
	resource_t *r;
	unsigned id;
	int err = EOK;

	proc_lockSet(&src->lock);
	for (n = lib_rbMinimum(src->resources.root); n != NULL; n = lib_rbNext(n)) {
		r = lib_treeof(resource_t, linkage, n);
		id = r->id;

		switch (r->type) {
		case rtLock:
			if ((err = proc_mutexCreate(&id)) < 0) {
				proc_lockClear(&src->lock);
				return err;
			}
			break;
		case rtCond:
			if ((err = proc_condCreate(&id)) < 0){
				proc_lockClear(&src->lock);
				return err;
			}
			break;
		case rtFile:
			if ((err = proc_fileAdd(&id, &r->oid)) < 0) {
				proc_lockClear(&src->lock);
				return err;
			}
			proc_open(r->oid);
			proc_fileSet(id, 2, NULL, r->offs);
			break;
		case rtInth:
			/* Don't copy interrupt handler */
			break;
		}
	}
	proc_lockClear(&src->lock);
	return EOK;
}


resource_t *resource_get(process_t *process, unsigned int id)
{
	resource_t *r, t;

	t.id = id;

	proc_lockSet(&process->lock);
	if ((r = lib_treeof(resource_t, linkage, lib_rbFind(&process->resources, &t.linkage))) != NULL)
		r->refs++;
	proc_lockClear(&process->lock);

	return r;
}


void resource_put(process_t *process, resource_t *r)
{
	proc_lockSet(&process->lock);
	if (r->refs)
		r->refs--;
	proc_lockClear(&process->lock);
	return;
}


int proc_resourceFree(unsigned int h)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;
	r = resource_get(process, h);

	return resource_free(r);
}


void resource_init(process_t *process)
{
	lib_rbInit(&process->resources, resource_cmp, resource_augment);
}
