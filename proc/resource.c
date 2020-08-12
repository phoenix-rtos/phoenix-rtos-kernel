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
#include "../include/errno.h"
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

	return (r1->id > r2->id) - (r1->id < r2->id);
}


static unsigned _resource_alloc(rbtree_t *tree, unsigned id)
{
	resource_t *r = lib_treeof(resource_t, linkage, tree->root);

	while (r != NULL) {
		if (r->lgap && id < r->id) {
			if (r->linkage.left == NULL)
				return max(id, r->id - r->lgap);

			r = lib_treeof(resource_t, linkage, r->linkage.left);
			continue;
		}

		if (r->rgap) {
			if (r->linkage.right == NULL)
				return max(id, r->id + 1);

			r = lib_treeof(resource_t, linkage, r->linkage.right);
			continue;
		}

		for (;; r = lib_treeof(resource_t, linkage, r->linkage.parent)) {
			if (r->linkage.parent == NULL)
				return NULL;

			if ((r == lib_treeof(resource_t, linkage, r->linkage.parent->left)) && lib_treeof(resource_t, linkage, r->linkage.parent)->rgap)
				break;
		}
		r = lib_treeof(resource_t, linkage, r->linkage.parent);

		if (r->linkage.right == NULL)
			return r->id + 1;

		r = lib_treeof(resource_t, linkage, r->linkage.right);
	}

	return id;
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

		n->lgap = !!((n->id <= p->id) ? n->id : n->id - p->id - 1);
	}
	else {
		l = lib_treeof(resource_t, linkage, node->left);
		n->lgap = max((int)l->lgap, (int)l->rgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(resource_t, linkage, it->parent);
			if (it->parent->left == it)
				break;
		}

		n->rgap = !!((n->id >= p->id) ? MAX_PID - n->id - 1 : p->id - n->id - 1);
	}
	else {
		r = lib_treeof(resource_t, linkage, node->right);
		n->rgap = max((int)r->lgap, (int)r->rgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(resource_t, linkage, it);
		p = lib_treeof(resource_t, linkage, it->parent);

		if (it->parent->left == it)
			p->lgap = max((int)n->lgap, (int)n->rgap);
		else
			p->rgap = max((int)n->lgap, (int)n->rgap);
	}
}


unsigned resource_alloc(process_t *process, resource_t *r, int type)
{
	r->type = type;
	r->refs = 2;

	proc_lockSet(&process->lock);
	r->id = _resource_alloc(&process->resources, 1);
	lib_rbInsert(&process->resources, &r->linkage);
	proc_lockClear(&process->lock);

	return r->id;
}


resource_t *resource_get(process_t *process, int type, unsigned int id)
{
	resource_t *r, t;
	t.id = id;

	proc_lockSet(&process->lock);
	if ((r = lib_treeof(resource_t, linkage, lib_rbFind(&process->resources, &t.linkage))) != NULL && r->type == type)
		lib_atomicIncrement(&r->refs);
	proc_lockClear(&process->lock);

	return r;
}


int resource_put(resource_t *r)
{
	return lib_atomicDecrement(&r->refs);
}


void resource_unlink(process_t *process, resource_t *r)
{
	proc_lockSet(&process->lock);
	lib_atomicDecrement(&r->refs);
	lib_rbRemove(&process->resources, &r->linkage);
	proc_lockClear(&process->lock);
}


resource_t *resource_remove(process_t *process, unsigned id)
{
	resource_t *r, t;
	t.id = id;

	proc_lockSet(&process->lock);
	if ((r = lib_treeof(resource_t, linkage, lib_rbFind(&process->resources, &t.linkage))) != NULL)
		lib_rbRemove(&process->resources, &r->linkage);
	proc_lockClear(&process->lock);

	return r;
}


resource_t *resource_removeNext(process_t *process)
{
	resource_t *r;

	proc_lockSet(&process->lock);
	if ((r = lib_treeof(resource_t, linkage, lib_rbMinimum(process->resources.root))) != NULL)
		lib_rbRemove(&process->resources, &r->linkage);
	proc_lockClear(&process->lock);

	return r;
}


void _resource_init(process_t *process)
{
	lib_rbInit(&process->resources, resource_cmp, resource_augment);
}


int proc_resourcePut(resource_t *r)
{
	int rem = -EINVAL;

	switch (r->type) {
	case rtLock:
		rem = mutex_put(resourceof(mutex_t, resource, r));
		break;

	case rtCond:
		rem = cond_put(resourceof(cond_t, resource, r));
		break;

	case rtInth:
		rem = userintr_put(resourceof(userintr_t, resource, r));
		break;
	}

	return rem;
}


int proc_resourceDestroy(process_t *process, unsigned id)
{
	resource_t *r;

	if ((r = resource_remove(process, id)) == NULL)
		return -EINVAL;

	if (proc_resourcePut(r))
		return -EBUSY;

	return EOK;
}


void proc_resourcesDestroy(process_t *process)
{
	resource_t *r;

	while ((r = resource_removeNext(process)))
		proc_resourcePut(r);
}


int proc_resourcesCopy(process_t *source)
{
	process_t *process = proc_current()->process;
	rbnode_t *n;
	resource_t *r, *newr;
	int err = EOK;

	proc_lockSet(&source->lock);
	for (n = lib_rbMinimum(source->resources.root); n != NULL; n = lib_rbNext(n)) {
		r = lib_treeof(resource_t, linkage, n);

		switch (r->type) {
		case rtLock:
			err = proc_mutexCreate();
			break;

		case rtCond:
			err = proc_condCreate();
			break;

		case rtInth:
			/* Don't copy interrupt handlers */
			err = EOK;
			break;

		default:
			err = -EINVAL;
			break;
		}

		if (err > 0 && err != r->id) {
			/* Reinsert resource to match original resource id */
			newr = resource_remove(process, err);
			newr->id = r->id;
			err = lib_rbInsert(&process->resources, &newr->linkage);
		}

		if (err < 0)
			break;
	}
	proc_lockClear(&source->lock);

	if (err > 0)
		err = EOK;

	return err;
}
