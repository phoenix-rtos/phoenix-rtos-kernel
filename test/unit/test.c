/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for proc subsystem
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
// #include <stdio.h>
// #define SIZE_T_DEFINED 1

// #include <stdio.h>
#include "fff.h"


#include <arch/types.h>
#include "../../posix/posix.h"
#include "../../posix/posix_private.h"
#include "../../proc/threads.h"
#include "../../include/msg.h"

#include "../../lib/rb.h"

DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(int, log_write, const char *, size_t);

FAKE_VOID_FUNC(hal_memcpy, void *, const void *, size_t);
FAKE_VALUE_FUNC(int, hal_memcmp, const void *, const void *, size_t);
FAKE_VOID_FUNC(hal_memset, void *, int, size_t);
FAKE_VALUE_FUNC(size_t, hal_strlen, const char *);
FAKE_VALUE_FUNC(int, hal_strcmp, const char *, const char *);
FAKE_VALUE_FUNC(int, hal_strncmp, const char *, const char *, size_t);
FAKE_VALUE_FUNC(char *, hal_strcpy, char *, const char *);
FAKE_VALUE_FUNC(char *, hal_strncpy, char *, const char *, size_t);
FAKE_VALUE_FUNC(unsigned long, hal_i2s, char *, char *, unsigned long, unsigned char, char);


FAKE_VALUE_FUNC(rbnode_t *, lib_rbFind, rbtree_t *, rbnode_t *);
FAKE_VALUE_FUNC(int, proc_lockSet, lock_t *);
FAKE_VALUE_FUNC(int, proc_lockClear, lock_t *);
FAKE_VOID_FUNC(lib_rbRemove, rbtree_t *, rbnode_t *);
FAKE_VOID_FUNC(vm_kfree, void *);
FAKE_VALUE_FUNC(int, proc_lockDone, lock_t *);

FAKE_VALUE_FUNC(int, proc_close, oid_t, unsigned);
FAKE_VALUE_FUNC(thread_t *, proc_current);
FAKE_VALUE_FUNC(void *, vm_kmalloc, size_t);
FAKE_VALUE_FUNC(int, proc_lockInit, lock_t *, const char *);
FAKE_VALUE_FUNC(int, proc_send, u32, msg_t *);
FAKE_VALUE_FUNC(int, lib_rbInsert, rbtree_t *, rbnode_t *);
FAKE_VOID_FUNC(lib_splitname, char *, char **, char **);
FAKE_VALUE_FUNC(int, proc_lookup, const char *, oid_t *, oid_t *);
FAKE_VALUE_FUNC(int, proc_create, int, int, int, oid_t, oid_t, char *, oid_t *);
FAKE_VALUE_FUNC(int, proc_open, oid_t, unsigned);
FAKE_VALUE_FUNC(off_t, proc_size, oid_t);
FAKE_VALUE_FUNC(int, proc_read, oid_t, off_t, void *, size_t, unsigned);
FAKE_VALUE_FUNC(int, proc_write, oid_t, off_t, void *, size_t, unsigned);
FAKE_VALUE_FUNC(int, proc_link, oid_t, oid_t, const char *);
FAKE_VALUE_FUNC(char *, lib_strdup, const char *);

FAKE_VOID_FUNC(lib_listAdd, void **, void *, size_t, size_t);
FAKE_VALUE_FUNC(rbnode_t *, lib_rbNext, rbnode_t *);
// FAKE_VALUE_FUNC(int, inet_setfl(unsigned socket, int flags);
// FAKE_VALUE_FUNC(int, proc_unlink(oid_t dir, oid_t oid, const char *name);
// FAKE_VALUE_FUNC(int, inet_getfl(unsigned socket);
// FAKE_VALUE_FUNC(int, inet_socket(int domain, int type, int protocol);
// FAKE_VALUE_FUNC(int, inet_accept4(unsigned socket, struct sockaddr *address, socklen_t *address_len, int flags);
// FAKE_VALUE_FUNC(int, inet_bind(unsigned socket, const struct sockaddr *address, socklen_t address_len);
// FAKE_VALUE_FUNC(int, inet_connect(unsigned socket, const struct sockaddr *address, socklen_t address_len);
// FAKE_VALUE_FUNC(int, inet_getpeername(unsigned socket, struct sockaddr *address, socklen_t *address_len);
// FAKE_VALUE_FUNC(int, inet_getsockname(unsigned socket, struct sockaddr *address, socklen_t *address_len);
// FAKE_VALUE_FUNC(int, inet_getsockopt(unsigned socket, int level, int optname, void *optval, socklen_t *optlen);
// FAKE_VALUE_FUNC(int, inet_listen(unsigned socket, int backlog);
// FAKE_VALUE_FUNC(ssize_t, inet_recvfrom(unsigned socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);
// FAKE_VALUE_FUNC(ssize_t, inet_sendto(unsigned socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
// FAKE_VALUE_FUNC(ssize_t, inet_recvmsg(unsigned socket, struct msghdr *msg, int flags);
// FAKE_VALUE_FUNC(ssize_t, inet_sendmsg(unsigned socket, const struct msghdr *msg, int flags);
// FAKE_VALUE_FUNC(int, inet_shutdown(unsigned socket, int how);
// FAKE_VALUE_FUNC(int, inet_setsockopt(unsigned socket, int level, int optname, const void *optval, socklen_t optlen);


FAKE_VALUE_FUNC(rbnode_t *, lib_rbFindEx, rbnode_t *, rbnode_t *, rbcomp_t);
FAKE_VALUE_FUNC(int, _cbuffer_init, cbuffer_t *, void *, size_t);
FAKE_VALUE_FUNC(int, _cbuffer_read, cbuffer_t *, void *, size_t);
FAKE_VALUE_FUNC(int, _cbuffer_write, cbuffer_t *, const void *, size_t);

FAKE_VOID_FUNC(hal_spinlockCreate, spinlock_t *, const char *);
FAKE_VOID_FUNC(hal_spinlockDestroy, spinlock_t *);

FAKE_VOID_FUNC(hal_spinlockSet, spinlock_t *, spinlock_ctx_t *);
FAKE_VOID_FUNC(hal_spinlockClear, spinlock_t *, spinlock_ctx_t *);
FAKE_VOID_FUNC(_hal_spinlockInit);

FAKE_VALUE_FUNC(int, proc_threadWait, thread_t **, spinlock_t *, time_t, spinlock_ctx_t *);
FAKE_VALUE_FUNC(int, proc_threadWakeup, thread_t **);

FAKE_VOID_FUNC(lib_listRemove, void **, void *, size_t, size_t);
FAKE_VOID_FUNC(lib_rbInit, rbtree_t *, rbcomp_t, rbaugment_t);

FAKE_VALUE_FUNC(int, proc_unlink, oid_t, oid_t, const char *);

FAKE_VOID_FUNC(proc_gettime, time_t *, time_t *);
FAKE_VALUE_FUNC(int, proc_threadSleep, time_t);
FAKE_VALUE_FUNC(process_t *, proc_find, unsigned int);
FAKE_VALUE_FUNC(int, threads_sigpost, process_t *, thread_t *, int);
FAKE_VOID_FUNC(threads_put, thread_t *);
FAKE_VALUE_FUNC(int, proc_put, process_t *);
FAKE_VALUE_FUNC(int, proc_sigpost, int, int);
FAKE_VOID_FUNC_VARARG(lib_assertPanic, const char *, int, const char *, ...);
// void lib_assertPanic(const char *func, int line, const char *fmt, ...)

FAKE_VALUE_FUNC(int, proc_lockWait, struct _thread_t **, lock_t *, time_t);
FAKE_VALUE_FUNC(int, lib_listBelongs, void **, void *, size_t, size_t);
FAKE_VALUE_FUNC(int, proc_threadBroadcast, thread_t **);
FAKE_VALUE_FUNC(int, proc_lockSet2, lock_t *, lock_t *);

FAKE_VALUE_FUNC(thread_t *, threads_findThread, int);
FAKE_VALUE_FUNC(rbnode_t *, lib_rbMinimum, rbnode_t *);




int main(void)
{
	// printf("hello\n");
	msg_t msg;
	unsigned long request = 7, ret;
	void *data = NULL;
	// hal_memcpy_fake.return_val = NULL;
	ret = ioctl_processResponse(&msg, request, data);
	// if(ret == NULL) {
	// 	return 5;
	// } else {
	// 	return 1;
	// }

	return ret;
}