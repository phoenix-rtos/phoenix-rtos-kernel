/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Kernel initialization
 *
 * Copyright 2012-2017, 2021 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"

#include "usrv.h"
#include "lib/lib.h"
#include "vm/vm.h"
#include "proc/proc.h"
#include "posix/posix.h"
#include "syscalls.h"
#include "syspage.h"
#include "test/test.h"


struct {
	vm_map_t kmap;
	vm_object_t kernel;
	page_t *page;
	void *stack;
	size_t stacksz;
} main_common;

extern void pmap_switch_bulktest(int cacheopt);

void main_initthr(void *unused)
{
	int res;
	unsigned int argc;

	syspage_prog_t *prog;
	char *argv[32], *cmdline;

	/* Enable locking and multithreading related mechanisms */
	_hal_start();
	_usrv_start();

	pmap_switch_bulktest(0);
	pmap_switch_bulktest(1);
	pmap_switch_bulktest(2);


	lib_printf("main: Starting syspage programs:");
	syspage_progShow();

	posix_init();
	posix_clone(-1);

	/* Start programs from syspage */
	if ((prog = syspage_progList()) != NULL) {
		do {
			cmdline = prog->argv;
			/* If app shouldn't be executed then args should be discarded */
			if (*cmdline != 'X') {
				while (*cmdline != ';' && *cmdline != '\0')
					++cmdline;

				*cmdline = '\0';
				continue;
			}

			/* 'X' is no longer useful */
			++prog->argv;
			cmdline = prog->argv;
			for (argc = 0; argc < (sizeof(argv) / sizeof(*argv) - 1);) {
				argv[argc++] = cmdline;
				while (*cmdline != ';' && *cmdline != '\0')
					++cmdline;

				if (*cmdline == '\0')
					break;

				*(cmdline++) = '\0';
			}
			argv[argc++] = NULL;

			if ((res = proc_syspageSpawn(prog, vm_getSharedMap(prog->imaps[0]), vm_getSharedMap(prog->dmaps[0]), argv[0], argv)) < 0)
				lib_printf("main: failed to spawn %s (%d)\n", argv[0], res);
		} while ((prog = prog->next) != syspage_progList());
	}

	for (;;) {
		proc_reap();
	}
}


int main(void)
{
	char s[128];

	syspage_init();
	_hal_init();
	_usrv_init();

	hal_consolePrint(ATTR_BOLD, "Phoenix-RTOS microkernel v. " RELEASE " rev. " VERSION "\n");
	lib_printf("hal: %s\n", hal_cpuInfo(s));
	lib_printf("hal: %s\n", hal_cpuFeatures(s, sizeof(s)));
	lib_printf("hal: %s\n", hal_interruptsFeatures(s, sizeof(s)));
	lib_printf("hal: %s\n", hal_timerFeatures(s, sizeof(s)));

	_vm_init(&main_common.kmap, &main_common.kernel);
	_proc_init(&main_common.kmap, &main_common.kernel);
	_syscalls_init();

#if 0 /* Basic kernel tests */
	/* Start tests */
	test_proc_threads1();
	test_vm_kmallocsim();
	test_proc_conditional();
	test_vm_alloc();
	test_vm_kmalloc();
	test_proc_exit();
#endif

	proc_start(main_initthr, NULL, (const char *)"init");

	/* Start scheduling, leave current stack */
	hal_cpuEnableInterrupts();
	hal_cpuReschedule(NULL, NULL);

	return 0;
}
