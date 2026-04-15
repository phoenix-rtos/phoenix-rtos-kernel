/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "lib/lib.h"
#include "proc.h"

#include "perf/trace-ipc.h"
#include "syscalls.h"


#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


/* clang-format off */
enum { msg_rejected = -1, msg_waiting = 0, msg_received, msg_responded };
/* clang-format on */


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


static cpu_context_t *_getUserContext(thread_t *thread)
{
	if (thread->process != NULL) {
		// if (hal_cpuSupervisorMode(thread->context) == 0) {
		return (cpu_context_t *)((char *)thread->kstack + thread->kstacksz - sizeof(cpu_context_t));
	}
	else {
		return thread->context;
	}
}

static void track_deprecated_msg(const char *func)
{
	thread_t *sender = proc_current();
	char name[26];
	if (sender->process != NULL) {
		process_getName(sender->process, name, sizeof(name));
	}

	LIB_ASSERT_ALWAYS(0, "%s called %s %p %p", func, name, _getUserContext(sender)->sepc, _getUserContext(sender)->ra);
}

/* TODO: move utcb init/deinit to threads */

msgBuf_t *proc_initMsgBuf(void)
{
	void *vaddr, *kvaddr;
	thread_t *t;
	vm_map_t *map;
	page_t *p;
	u8 prot, flags, attr;

	t = proc_current();

	if (t->utcb.kw != NULL) {
		LIB_ASSERT(t->utcb.w != NULL, "");
		LIB_ASSERT(t->utcb.p != NULL, "");
		t->utcb.kw->err = 0;
		t->utcb.kw->buf = NULL;
		t->utcb.kw->bufsize = 0;
		return t->utcb.w;
	}

	map = (t->process == NULL || t->process->mapp == NULL) ? msg_common.kmap : t->process->mapp;

	prot = PROT_WRITE | PROT_READ;
	flags = MAP_NOINHERIT;
	attr = PGHD_READ | PGHD_WRITE | PGHD_PRESENT | vm_flagsToAttr(flags);

	if (t->process != NULL) {
		prot |= PROT_USER;
		attr |= PGHD_USER;
	}

	p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
	if (p == NULL) {
		LIB_ASSERT(0, "page alloc failed");
		return NULL;
	}
	t->utcb.p = p;

	/* map to current thread space */
	vaddr = vm_mapFind(map, NULL, SIZE_PAGE, flags, prot);
	if (vaddr == NULL) {
		proc_freeUtcb(t);
		return NULL;
	}
	t->utcb.w = vaddr;

	if (page_map(&map->pmap, vaddr, p->addr, attr) < 0) {
		proc_freeUtcb(t);
		return NULL;
	}

	if (t->process != NULL) {
		/* map to kernel space */
		kvaddr = vm_mapFind(msg_common.kmap, NULL, SIZE_PAGE, flags, prot);
		if (kvaddr == NULL) {
			proc_freeUtcb(t);
			return NULL;
		}
		t->utcb.kw = kvaddr;

		if (page_map(&msg_common.kmap->pmap, kvaddr, p->addr, attr) < 0) {
			proc_freeUtcb(t);
			return NULL;
		}
	}
	else {
		/*
		 * else: this is a kernel thread, so t->utcb.w is already mapped to kernel
		 * space
		 */
		t->utcb.kw = t->utcb.w;
	}

	t->utcb.kw->err = 0;
	t->utcb.kw->buf = NULL;
	t->utcb.kw->bufsize = 0;

	return vaddr;
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
}
