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

#include "klog/klog.h"
#include "lib/lib.h"
#include "vm/vm.h"
#include "proc/proc.h"
#include "posix/posix.h"
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
	size_t i;
	syspage_program_t prog;
	int xcount = 0, res;
	char *cmdline = syspage->arg, *end;
	char *argv[32], *arg, *argend;

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

	while (cmdline != NULL && *cmdline != '\0') {
		end = cmdline;
		while (*end && *(++end) != ' ');
		while (*end == ' ')
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
			for (i = 0; i < syspage->progssz; i++) {
				if (!hal_strcmp(cmdline + 1, syspage->progs[i].cmdline)) {
					argv[0] = syspage->progs[i].cmdline;
					hal_memcpy(&prog, &syspage->progs[i], sizeof(prog));
					res = proc_syspageSpawn(&prog, vm_getSharedMap(&prog, -1), prog.cmdline, argv);
					if (res < 0)
						lib_printf("main: failed to spawn %s (%d)\n", argv[0], res);
				}
			}
		}

		cmdline = end;
	}

	if (!xcount && syspage->progssz != 0) {
		argv[1] = NULL;
		/* Start all syspage programs */
		for (i = 0; i < syspage->progssz; i++) {
			argv[0] = syspage->progs[i].cmdline;
			hal_memcpy(&prog, &syspage->progs[i], sizeof(prog));
			res = proc_syspageSpawn(&prog, vm_getSharedMap(&prog, -1), prog.cmdline, argv);
			if (res < 0) {
				lib_printf("main: failed to spawn %s (%d)\n", argv[0], res);
			}
		}
	}

	/* Reopen stdin, stdout, stderr */
//	proc_lookup("/dev/console", &oid);

//	proc_fileSet(0, 3, &oid, 0, 0);
//	proc_fileSet(1, 3, &oid, 0, 0);
//	proc_fileSet(2, 3, &oid, 0, 0);

	for (;;)
		proc_reap();
}


int main(void)
{
	char s[128];

	_hal_init();

	_klog_init();
	hal_consolePrint(ATTR_BOLD, "Phoenix-RTOS microkernel v. " VERSION "\n");
	lib_printf("hal: %s\n", hal_cpuInfo(s));
	lib_printf("hal: %s\n", hal_cpuFeatures(s, sizeof(s)));
	lib_printf("hal: %s\n", hal_interruptsFeatures(s, sizeof(s)));

	_vm_init(&main_common.kmap, &main_common.kernel);
	_proc_init(&main_common.kmap, &main_common.kernel);
	_syscalls_init();

	_klog_initSrv();

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
	hal_cpuReschedule(NULL, NULL);

	return 0;
}
