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


/* clang-format off */
#define SYSCALLS(ID) \
	ID(debug) \
	ID(sys_mmap) \
	ID(sys_munmap) \
	ID(sys_fork) \
	ID(vforksvc) \
	ID(exec) \
	ID(spawnSyspage) \
	ID(sys_exit) \
	ID(sys_waitpid) \
	ID(threadJoin) \
	ID(getpid) \
	ID(getppid) \
	ID(gettid) \
	ID(beginthreadex) \
	ID(endthread) \
	ID(nsleep) \
	ID(phMutexCreate) \
	ID(phMutexLock) \
	ID(mutexTry) \
	ID(mutexUnlock) \
	ID(phCondCreate) \
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
	ID(sys_open) \
	ID(sys_close) \
	ID(sys_link) \
	ID(sys_unlink) \
	ID(sys_fcntl) \
	ID(sys_ftruncate) \
	ID(sys_lseek) \
	ID(sys_dup) \
	ID(sys_dup2) \
	ID(sys_pipe) \
	ID(sys_mkfifo) \
	ID(sys_chmod) \
	ID(sys_fstat) \
	ID(sys_fsync) \
	\
	ID(sys_accept) \
	ID(sys_accept4) \
	ID(sys_bind) \
	ID(sys_connect) \
	ID(sys_gethostname) \
	ID(sys_getpeername) \
	ID(sys_getsockname) \
	ID(sys_getsockopt) \
	ID(sys_listen) \
	ID(sys_recvfrom) \
	ID(sys_sendto) \
	ID(sys_recvmsg) \
	ID(sys_sendmsg) \
	ID(sys_socket) \
	ID(sys_socketpair) \
	ID(sys_shutdown) \
	ID(sys_sethostname) \
	ID(sys_setsockopt) \
	\
	ID(sys_ioctl) \
	ID(sys_futimens) \
	ID(sys_poll) \
	\
	ID(sys_tkill) \
	\
	ID(sys_setpgid) \
	ID(sys_getpgid) \
	ID(sys_setpgrp) \
	ID(sys_getpgrp) \
	ID(sys_setsid) \
	ID(sys_spawn) \
	ID(release) \
	ID(sbi_putchar) \
	ID(sbi_getchar) \
	ID(sigreturn) \
	\
	ID(sys_mprotect) \
	\
	ID(sys_statvfs) \
	ID(sys_uname)
/* clang-format on */
