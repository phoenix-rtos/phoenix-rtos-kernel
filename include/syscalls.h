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
	ID(fork) \
	ID(vforksvc) \
	ID(exec) \
	ID(exit) \
	ID(waitpid) \
	ID(getpid) \
	ID(getppid) \
	ID(gettid) \
	ID(beginthreadex) \
	ID(endthread) \
	ID(usleep) \
	ID(mutexCreate) \
	ID(mutexLock) \
	ID(mutexTry) \
	ID(mutexUnlock) \
	ID(condCreate) \
	ID(condWait) \
	ID(condSignal) \
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
	ID(fileAdd) \
	ID(fileSet) \
	ID(fileGet) \
	ID(fileRemove) \
	ID(threadsinfo) \
	ID(meminfo) \
	ID(syspageprog) \
	ID(va2pa) \
	ID(signalHandle) \
	ID(signalPost) \
	ID(signalReturn) \
	ID(signalMask) \
	ID(priority) \
	\
	ID(read) \
	ID(write) \
	ID(open_absolute) \
	ID(close) \
	ID(link_absolute) \
	ID(unlink_absolute) \
	ID(fcntl) \
	ID(ftruncate) \
	ID(lseek) \
	ID(dup) \
	ID(dup2) \
	ID(pipe) \
	ID(mkfifo_absolute) \
	ID(fstat) \
	\
	ID(accept) \
	ID(bind) \
	ID(connect) \
	ID(getpeername) \
	ID(getsockname) \
	ID(getsockopt) \
	ID(listen) \
	ID(recvfrom) \
	ID(sendto) \
	ID(socket) \
	ID(shutdown) \
	ID(setsockopt) \
	\
	ID(ioctl) \
	ID(utimes)
