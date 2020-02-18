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
	ID(memMap) \
	ID(memUnmap) \
	ID(ProcFork) \
	ID(vforksvc) \
	ID(ProcExec) \
	ID(ProcExit) \
	ID(ProcWait) \
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
	ID(portRegister) \
	ID(portEvent) \
	ID(msgSend) \
	ID(portRecv) \
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
	ID(sys_openat) \
	ID(sys_open) \
	ID(sys_close) \
	ID(fileRead) \
	ID(fileWrite) \
	ID(fileLink) \
	ID(fileUnlink) \
	ID(sys_fcntl) \
	ID(sys_ftruncate) \
	ID(fileSeek) \
	ID(procChangeDir) \
	ID(sys_dup3) \
	ID(sys_fchmod) \
	ID(fileStat) \
	ID(fileIoctl) \
	ID(sys_spawn) \
	ID(release) \
	ID(SetRoot) \
	\
	ID(sys_poll)\
	ID(sys_pipe)\
	ID(fifoCreate)\
	\
	ID(sys_accept4) \
	ID(sys_bind) \
	ID(sys_connect) \
	ID(sys_getpeername) \
	ID(sys_getsockname) \
	ID(sys_getsockopt) \
	ID(sys_listen) \
	ID(sys_recvfrom) \
	ID(sys_sendto) \
	ID(sys_socket) \
	ID(sys_shutdown) \
	ID(sys_setsockopt) \
	ID(sys_setpgid) \
	ID(sys_getpgid) \
	ID(sys_setsid) \
	ID(sys_getsid) \
	ID(threadKill) \
	ID(threadSleep) \
	ID(deviceCreate) \
	ID(fsMount) \
	ID(fsBind) \
	ID(portGet) \
	ID(sys_recvmsg) \
	ID(sys_sendmsg) \
	ID(sys_socketPair) \
	ID(sys_sigaction)
