/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Kernel initialization
 *
 * Copyright 2012-2017 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL

#include "lib/lib.h"
#include "vm/vm.h"
#include "proc/proc.h"
#include "syscalls.h"
#include "test/test.h"
#include "programs.h"


struct {
	vm_map_t kmap;
	vm_object_t kernel;
	page_t *page;
	void *stack;
	size_t stacksz;
} main_common;


void main_initthr(void *unused)
{
	int i;
	syspage_program_t *prog, *last = NULL;
	oid_t oid = { 0, 0 };
	int xcount = 0;
	char *cmdline = syspage->arg, *end;
	char *argv[32], *arg, *argend;
	time_t sleep;

	/* Enable locking and multithreading related mechanisms */
	_hal_start();

	lib_printf("main: Decoding programs from data segment\n");
	programs_decode(&main_common.kmap, &main_common.kernel);

	lib_printf("main: Starting syspage programs:");
	for (i = 0; i < syspage->progssz; i++)
		lib_printf(" '%s',", syspage->progs[i].cmdline);
	lib_printf("\b \n");

	posix_init();
	posix_clone(-1);

	/* Free memory used by initial stack */
	/*vm_munmap(&main_common.kmap, main_common.stack, main_common.stacksz);
	vm_pageFree(p);*/

	/* Set stdin, stdout, stderr ports */
//	proc_fileAdd(&h, &oid, 0);
//	proc_fileAdd(&h, &oid, 0);
//	proc_fileAdd(&h, &oid, 0);

	argv[0] = NULL;

	while (*cmdline) {
		end = cmdline;
		while (*end && *(++end) != ' ');
		while (*end && *end == ' ')
			*(end++) = 0;
		if (*cmdline == 'X' && ++xcount) {
			i = 0;
			argend = cmdline;

			while (i < sizeof(argv) / sizeof(*argv) - 1) {
				arg = ++argend;
				while (*argend && *argend != ';')
					argend++;

				argv[i++] = arg;

				if (!*argend)
					break;

				*argend = 0;
			}
			argv[i++] = NULL;

			if (i == sizeof(argv) / sizeof(*argv))
				lib_printf("main: truncated arguments for command '%s'\n", argv[0]);

			/* Start program loaded into memory */
			for (prog = syspage->progs, i = 0; i < syspage->progssz; i++, prog++) {
				if (!hal_strcmp(cmdline + 1, prog->cmdline)) {
					if (!*end)
						last = prog;
					else if (!proc_vfork())
						proc_execve(prog, prog->cmdline, argv, NULL);
				}
			}
		}

		cmdline = end;
	}

	if (!xcount && syspage->progssz != 0) {
		/* Start all syspage programs */
		for (prog = syspage->progs, i = 0; i < syspage->progssz - 1; i++, prog++) {
			if (!proc_vfork())
				proc_execle(prog, prog->cmdline, prog->cmdline, NULL, NULL);
		}
		last = prog;
	}

	/* Reopen stdin, stdout, stderr */
//	proc_lookup("/dev/console", &oid);

//	proc_fileSet(0, 3, &oid, 0, 0);
//	proc_fileSet(1, 3, &oid, 0, 0);
//	proc_fileSet(2, 3, &oid, 0, 0);

	if (last != NULL) {
		if (!proc_vfork())
			proc_execve(last, last->cmdline, argv, NULL);
	}

	sleep = 10000;
	proc_fileGet(0, 1, &oid, 0, NULL);
	while (proc_write(oid, 0, "", 1, 0) < 0) {
		proc_fileGet(0, 1, &oid, 0, NULL);
		proc_threadSleep(sleep);

		if ((sleep *= 2) > 2000000)
			sleep = 2000000;
	}

	sleep = 10000;
	while (proc_lookup("/", &oid) < 0) {
		proc_threadSleep(sleep);

		if ((sleep *= 2) > 2000000)
			sleep = 2000000;
	}

	/* Initialize system */
	proc_execle(NULL, "/bin/init", "init", NULL, NULL);
	proc_execle(NULL, "/sbin/busybox", "/sbin/busybox", "ash", NULL, NULL);
	proc_execle(NULL, "/bin/psh", "/bin/psh", NULL, NULL);

	for (;;)
		proc_threadSleep(2000000);

}


int main(void)
{
	char s[128];

	_hal_init();

	hal_consolePrint(ATTR_BOLD, "Phoenix-RTOS microkernel v. " VERSION "\n");
	lib_printf("hal: %s\n", hal_cpuInfo(s));
	lib_printf("hal: %s\n", hal_cpuFeatures(s, sizeof(s)));

	_vm_init(&main_common.kmap, &main_common.kernel);
	_proc_init(&main_common.kmap, &main_common.kernel);
	_syscalls_init();

	/* Start tests */

	/*
	test_proc_threads1();
	test_vm_kmallocsim();
	test_proc_conditional();
	test_vm_alloc();
	test_vm_kmalloc();
	test_proc_exit();
	*/

	proc_start(main_initthr, NULL, (const char *)"init");

	/* Start scheduling, leave current stack */
	hal_cpuEnableInterrupts();
	hal_cpuReschedule(NULL);

	return 0;
}
