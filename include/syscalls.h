/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System calls definitions
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define SYSCALLS(ID) \
	ID(0, syscalls_klog, debug) \
	ID(1, syscalls_mmap, mmap) \
	ID(2, syscalls_munmap, munmap) \
	ID(3, syscalls_fork, fork) \
	ID(4, syscalls_vfork, vforksvc) \
	ID(5, syscalls_execle, execle) \
	ID(6, syscalls_execve, execve) \
	ID(7, syscalls_exit, exit) \
	ID(8, syscalls_waitpid, waitpid) \
	ID(9, syscalls_getpid, getpid) \
	ID(10, syscalls_getppid, getppid) \
	ID(11, syscalls_beginthreadex, beginthreadex) \
	ID(12, syscalls_endthread, endthread) \
	ID(13, syscalls_usleep, usleep) \
	ID(14, syscalls_mutexCreate, mutexCreate) \
	ID(15, syscalls_mutexLock, mutexLock) \
	ID(16, syscalls_mutexTry, mutexTry) \
	ID(17, syscalls_mutexUnlock, mutexUnlock) \
	ID(18, syscalls_semaphoreCreate, semaphoreCreate) \
	ID(19, syscalls_semaphoreDown, semaphoreDown) \
	ID(20, syscalls_semaphoreUp, semaphoreUp) \
	ID(21, syscalls_condCreate, condCreate) \
	ID(22, syscalls_condWait, condWait) \
	ID(23, syscalls_condSignal, condSignal) \
	ID(24, syscalls_destroy, resourceDestroy) \
	ID(25, syscalls_interrupt, interrupt) \
	ID(26, syscalls_portCreate, portCreate) \
	ID(27, syscalls_portDestroy, portDestroy) \
	ID(28, syscalls_portRegister, portRegister) \
	ID(29, syscalls_send, msgSend) \
	ID(30, syscalls_recv, msgRecv) \
	ID(31, syscalls_respond, msgRespond) \
	ID(32, syscalls_lookup, lookup) \
	ID(33, syscalls_gettime, gettime) \
	ID(34, syscalls_keepidle, keepidle) \
	ID(35, syscalls_mmdump, mmdump) \
	ID(36, syscalls_platformctl, platformctl) \
	ID(37, syscalls_wdgReload, wdgreload) \
	ID(38, syscalls_fileAdd, fileAdd) \
	ID(39, syscalls_fileSet, fileSet) \
	ID(40, syscalls_fileGet, fileGet) \
	ID(41, syscalls_fileRemove, fileRemove) \
	ID(42, syscalls_threadsinfo, threadsinfo) \
	ID(43, syscalls_meminfo, meminfo) \
	ID(44, syscalls_syspageprog, syspageprog) \
	ID(45, syscalls_va2pa, va2pa) \
	ID(46, syscalls_signalHandle, signalHandle) \
	ID(47, syscalls_signalPost, signalPost) \
	ID(48, syscalls_signalReturn, signalReturn) \
	ID(49, syscalls_signalMask, signalMask)
