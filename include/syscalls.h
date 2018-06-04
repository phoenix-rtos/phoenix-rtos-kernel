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
	ID(18, syscalls_condCreate, condCreate) \
	ID(19, syscalls_condWait, condWait) \
	ID(20, syscalls_condSignal, condSignal) \
	ID(21, syscalls_destroy, resourceDestroy) \
	ID(22, syscalls_interrupt, interrupt) \
	ID(23, syscalls_portCreate, portCreate) \
	ID(24, syscalls_portDestroy, portDestroy) \
	ID(25, syscalls_portRegister, portRegister) \
	ID(26, syscalls_send, msgSend) \
	ID(27, syscalls_recv, msgRecv) \
	ID(28, syscalls_respond, msgRespond) \
	ID(29, syscalls_lookup, lookup) \
	ID(30, syscalls_gettime, gettime) \
	ID(31, syscalls_keepidle, keepidle) \
	ID(32, syscalls_mmdump, mmdump) \
	ID(33, syscalls_platformctl, platformctl) \
	ID(34, syscalls_wdgReload, wdgreload) \
	ID(35, syscalls_fileAdd, fileAdd) \
	ID(36, syscalls_fileSet, fileSet) \
	ID(37, syscalls_fileGet, fileGet) \
	ID(38, syscalls_fileRemove, fileRemove) \
	ID(39, syscalls_threadsinfo, threadsinfo) \
	ID(40, syscalls_meminfo, meminfo) \
	ID(41, syscalls_syspageprog, syspageprog) \
	ID(42, syscalls_va2pa, va2pa) \
	ID(43, syscalls_signalHandle, signalHandle) \
	ID(44, syscalls_signalPost, signalPost) \
	ID(45, syscalls_signalReturn, signalReturn) \
	ID(46, syscalls_signalMask, signalMask)
