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
	ID(debug) \
	ID(mmap) \
	ID(munmap) \
	ID(sys_fork) \
	ID(vforksvc) \
	ID(exec) \
	ID(sys_exit) \
	ID(sys_waitpid) \
	ID(threadJoin) \
	ID(getpid) \
	ID(getppid) \
	ID(gettid) \
	ID(beginthreadex) \
	ID(endthread) \
	ID(usleep) \
	ID(mutexCreate) \
	ID(phMutexLock) \
	ID(mutexTry) \
	ID(mutexUnlock) \
	ID(condCreate) \
	ID(phCondWait) \
	ID(condSignal) \
	ID(condBroadcast) \
	ID(resourceDestroy) \
	ID(interrupt) \
	ID(portCreate) \
	ID(portDestroy) \
	ID(portRegister) \
	ID(msgSend) \
	ID(msgRecv) \
	ID(msgRespond) \
	ID(lookup) \
	ID(gettime) \
	ID(settime) \
	ID(keepidle) \
	ID(mmdump) \
	ID(platformctl) \
	ID(wdgreload) \
	ID(threadsinfo) \
	ID(meminfo) \
	ID(perf_start) \
	ID(perf_read) \
	ID(perf_finish) \
	ID(syspageprog) \
	ID(va2pa) \
	ID(signalHandle) \
	ID(signalPost) \
	ID(signalMask) \
	ID(signalSuspend) \
	ID(priority) \
	\
	ID(sys_read) \
	ID(sys_write) \
	ID(sys_openat) \
	ID(sys_open) \
	ID(sys_close) \
	ID(sys_linkat) \
	ID(sys_unlinkat) \
	ID(sys_fcntl) \
	ID(sys_ftruncate) \
	ID(lseek) \
	ID(sys_dup3) \
	ID(sys_fchmod) \
	ID(sys_fstat) \
	ID(sys_ioctl) \
	ID(sys_spawn) \
	ID(release) \
	ID(SetRoot) \
	\
	ID(sys_setpgid) \
	ID(sys_setpgrp) \
	ID(sys_getpgid) \
	ID(sys_getpgrp) \
	ID(sys_setsid) \
	ID(sys_dup) \
	ID(sys_dup2) \
	ID(sys_pipe) \
	ID(sys_link) \
	ID(sys_unlink) \
	ID(sys_mkfifo) \
	ID(sys_chmod) \
	ID(sys_tkill) \
	\
	ID(sys_poll)


	// ID(setpgid)
	// ID(getpgid)
	// ID(setpgrp)
	// ID(getpgrp)
	// ID(setsid)

//	ID(pipe)
//	ID(mkfifo)
//	ID(utimes)
//	ID(tkill)
	// ID(accept4)
	// ID(bind)
	// ID(connect)
	// ID(getpeername)
	// ID(getsockname)
	// ID(getsockopt)
	// ID(listen)
	// ID(recvfrom)
	// ID(sendto)
	// ID(socket)
	// ID(shutdown)
	// ID(setsockopt)
