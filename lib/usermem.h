/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Safe routines for user memory handling
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PH_LIB_USERMEM_H_
#define _PH_LIB_USERMEM_H_

#include "arch/exceptions.h"
#include <proc/threads.h>
#include <hal/cpu.h>

#ifdef EXCJMP_SUPPORTED

/*
 * Runs try_block with a setjmp/longjmp-style guard: any CPU fault while
 * accessing user memory unwinds back and runs catch_block instead of panicking.
 * Due to specific setjmp compiler guarantees, some rules apply:
 *
 *  - Non-volatile locals written to inside try_block are indeterminate after
 *    the fault unwind.
 *  - Never return/break/continue/goto out of try_block; leaving before the
 *    unwind leaves excjmpctx pointing at a dead stack frame (catch_block is
 *    safe to escape, as it runs after the handler was restored).
 *  - Take no locks, spinlocks or allocations inside try_block - the fault path
 *    skips their release and rolls cpsr (irq mask) back out of sync with them.
 *    Acquire before the macro, release after it or inside catch_block.
 *  - Keep try_block as minimalistic as possible, to the user-memory access only,
 *    so it does not mask genuine kernel bugs as -EFAULT.
 */
#define USERMEM_TRY_EX(try_block, catch_block, excctx, oldctx, current_thread) \
	do { \
		if (hal_createexcjmp(excctx) == 0) { \
			(current_thread)->excjmpctx = (excctx); \
			(try_block); \
			(current_thread)->excjmpctx = (oldctx); \
		} \
		else { \
			(current_thread)->excjmpctx = (oldctx); \
			(catch_block); \
		} \
	} while (0)

#define USERMEM_TRY(try_block, catch_block) \
	do { \
		volatile excjmp_context_t excctx, *oldctx = threads_getexcjmp(); \
		if (hal_createexcjmp(&excctx) == 0) { \
			threads_setexcjmp(&excctx); \
			(try_block); \
			threads_setexcjmp(oldctx); \
		} \
		else { \
			threads_setexcjmp(oldctx); \
			(catch_block); \
		} \
	} while (0)

#else

#define USERMEM_TRY_EX(try_block, catch_block, excctx, oldctx, current_thread) (try_block);

#define USERMEM_TRY(try_block, catch_block) (try_block);

#endif


int usermem_memcpy(void *dst, const void *src, size_t l);


int usermem_strnlen(const char *str, size_t n);


int usermem_strndup(const char *str, size_t n, char **duplicate);


#endif
