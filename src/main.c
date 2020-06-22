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
	vm_map_t *maps[16];
} main_common;


static int main_createMap(ptr_t start, ptr_t stop, unsigned int attr, int no)
{
#ifdef NOMMU
	if (no > sizeof(main_common.maps) / sizeof(main_common.maps[0]))
		return -ENOMEM;

	/* TODO enable MPU */
	(void)attr;

	if (start <= stop || start & (SIZE_PAGE - 1) || stop & (SIZE_PAGE - 1)) {
		main_common.maps[no] = NULL;
		return -EINVAL;
	}

	if ((main_common.maps[no] = vm_kmalloc(sizeof(vm_map_t))) == NULL) {
		return -ENOMEM;
	}

	if (vm_mapCreate(main_common.maps[no], (void *)start, (void *)stop) < 0) {
		vm_kfree(main_common.maps[no]);
		main_common.maps[no] = NULL;
	}

	return EOK;
#endif
}


static vm_map_t *main_getmap(syspage_program_t *prog)
{
#ifdef NOMMU
	if (prog->mapno <= 0 || prog->mapno > sizeof(main_common.maps) / sizeof (main_common.maps[0]))
		return NULL;

	return main_common.maps[prog->mapno - 1];
#else
	return NULL;
#endif
}


void main_initthr(void *unused)
{
	size_t i;
	syspage_program_t *prog;
	int xcount = 0, mcount = 0, res;
	char *cmdline = syspage->arg, *end;
	char *argv[32], *arg, *argend;
	ptr_t start = 0, stop = 0;
	unsigned int t;

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

	for (i = 0; i < sizeof(main_common.maps) / sizeof(main_common.maps[0]); ++i)
		main_common.maps[i] = NULL;

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
		while (*end && *end == ' ')
			*(end++) = 0;
		if (*cmdline == 'X' && ++xcount) {
			argend = cmdline;

			for (i = 0; i < sizeof(argv) / sizeof(*argv) - 1; ++i) {
				arg = ++argend;
				while (*argend && *argend != ';')
					argend++;

				argv[i] = arg;

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
					argv[0] = prog->cmdline;
					res = proc_syspageSpawn(prog, main_getmap(prog), prog->cmdline, argv);
					if (res < 0) {
						lib_printf("main: failed to spawn %s (%d)\n", argv[0], res);
					}
				}
			}
		}
		else if (*cmdline == 'M') {
			argend = cmdline;
			for (i = 0; i < 3; ++i) {
				arg = ++argend;
				while (*argend != '\0' && *argend != ';')
					++argend;

				if (i < 2 && *argend == '\0') {
					lib_printf("main: Invalid memory map definition\n");
					break;
				}

				*argend = '\0';

				t = lib_strtoul(arg, NULL, 16);

				if (i == 0)
					start = t;
				else if (i == 1)
					stop = t;
				else if ((res = main_createMap(start, stop, t, mcount)) < 0)
					lib_printf("main: Memory map creation failed (%d)\n", res);
			}
		}

		cmdline = end;
	}

	if (!xcount && syspage->progssz != 0) {
		argv[1] = NULL;
		/* Start all syspage programs */
		for (prog = syspage->progs, i = 0; i < syspage->progssz; i++, prog++) {
				argv[0] = prog->cmdline;
				res = proc_syspageSpawn(prog, main_getmap(prog), prog->cmdline, argv);
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
