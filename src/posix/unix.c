/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module, UNIX sockets
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../include/errno.h"
#include "../proc/proc.h"
#include "../lib/lib.h"

#include "posix.h"
#include "posix_private.h"

#define US_BOUND (1 << 0)
#define US_LISTENING (1 << 1)
#define US_ACCEPTING (1 << 2)
#define US_CONNECTING (1 << 3)


typedef struct _unixsock_t {
	rbnode_t linkage;
	unsigned id;
	unsigned int lmaxgap;
	unsigned int rmaxgap;

	struct _unixsock_t *next, *prev;

	int refs;

	lock_t lock;
	cbuffer_t buffer;
	char type;
	char state;

	spinlock_t spinlock;

	struct _unixsock_t *connect;
	thread_t *queue;
	thread_t *writeq;
} unixsock_t;


static struct {
	rbtree_t tree;
	lock_t lock;
} unix_common;



static int unixsock_cmp(rbnode_t *n1, rbnode_t *n2)
{
	unixsock_t *r1 = lib_treeof(unixsock_t, linkage, n1);
	unixsock_t *r2 = lib_treeof(unixsock_t, linkage, n2);

	return (r1->id - r2->id);
}


static int unixsock_gapcmp(rbnode_t *n1, rbnode_t *n2)
{
	unixsock_t *r1 = lib_treeof(unixsock_t, linkage, n1);
	unixsock_t *r2 = lib_treeof(unixsock_t, linkage, n2);
	rbnode_t *child = NULL;
	int ret = 1;

	if (r1->lmaxgap > 0 && r1->rmaxgap > 0) {
		if (r2->id > r1->id) {
			child = n1->right;
			ret = -1;
		}
		else {
			child = n1->left;
			ret = 1;
		}
	}
	else if (r1->lmaxgap > 0) {
		child = n1->left;
		ret = 1;
	}
	else if (r1->rmaxgap > 0) {
		child = n1->right;
		ret = -1;
	}

	if (child == NULL)
		return 0;

	return ret;
}


static void unixsock_augment(rbnode_t *node)
{
	rbnode_t *it;
	unixsock_t *n = lib_treeof(unixsock_t, linkage, node);
	unixsock_t *p = n, *r, *l;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(unixsock_t, linkage, it->parent);
			if (it->parent->right == it)
				break;
		}

		n->lmaxgap = (n->id <= p->id) ? n->id : n->id - p->id - 1;
	}
	else {
		l = lib_treeof(unixsock_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(unixsock_t, linkage, it->parent);
			if (it->parent->left == it)
				break;
		}

		n->rmaxgap = (n->id >= p->id) ? (unsigned)-1 - n->id - 1 : p->id - n->id - 1;
	}
	else {
		r = lib_treeof(unixsock_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(unixsock_t, linkage, it);
		p = lib_treeof(unixsock_t, linkage, it->parent);

		if (it->parent->left == it)
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		else
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
	}
}


static unixsock_t *unixsock_alloc(unsigned *id, int type)
{
	unixsock_t *r, t;

	*id = 0;
	proc_lockSet(&unix_common.lock);
	if (unix_common.tree.root != NULL) {
		t.id = 0;
		r = lib_treeof(unixsock_t, linkage, lib_rbFindEx(unix_common.tree.root, &t.linkage, unixsock_gapcmp));
		if (r != NULL) {
			if (r->lmaxgap > 0)
				*id = r->id - 1;
			else
				*id = r->id + 1;
		}
		else {
			proc_lockClear(&unix_common.lock);
			return NULL;
		}
	}

	if ((r = vm_kmalloc(sizeof(unixsock_t))) == NULL) {
		proc_lockClear(&unix_common.lock);
		return NULL;
	}

	proc_lockInit(&r->lock);

	r->id = *id;
	r->refs = 1;
	r->type = type;
	r->connect = NULL;
	r->queue = NULL;
	r->writeq = NULL;
	r->state = 0;
	r->next = NULL;
	r->prev = NULL;
	hal_spinlockCreate(&r->spinlock, "unix socket");

	lib_rbInsert(&unix_common.tree, &r->linkage);
	proc_lockClear(&unix_common.lock);

	return r;
}


static unixsock_t *unixsock_get(unsigned id)
{
	unixsock_t *r, t;

	t.id = id;

	proc_lockSet(&unix_common.lock);
	if ((r = lib_treeof(unixsock_t, linkage, lib_rbFind(&unix_common.tree, &t.linkage))) != NULL)
		r->refs++;
	proc_lockClear(&unix_common.lock);

	return r;
}


static void unixsock_put(unixsock_t *r)
{
	proc_lockSet(&unix_common.lock);
	if (--r->refs < 0) {
		lib_rbRemove(&unix_common.lock, &r->linkage);
		proc_lockClear(&unix_common.lock);

		proc_lockDone(&r->lock);
		hal_spinlockDestroy(&r->spinlock);
		vm_kfree(r);
		return;
	}
	proc_lockClear(&unix_common.lock);
}


int unix_lookupSocket(const char *path)
{
	int err;
	oid_t oid;

	if ((err = proc_lookup(path, &oid)) < 0)
		return err;

	if (oid.port != US_PORT)
		return -ENOTSOCK;

	return (unsigned)oid.id;
}


int unix_socket(int domain, int type, int protocol)
{
	unixsock_t *s;
	unsigned id;

	if (type != SOCK_STREAM && type != SOCK_DGRAM)
		return -EINVAL;

	if ((s = unixsock_alloc(&id, type)) == NULL)
		return -ENOMEM;

	return id;
}


int unix_accept(unsigned socket, struct sockaddr *address, socklen_t *address_len)
{
	unixsock_t *s, *conn, *new;
	int err;
	unsigned newid;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->type != SOCK_STREAM && s->type != SOCK_SEQPACKET) {
			err = -EOPNOTSUPP;
			break;
		}

		if (!(s->state & US_LISTENING)) {
			err = -EINVAL;
			break;
		}

		if ((new = unixsock_alloc(&newid, s->type)) == NULL) {
			err = -ENOMEM;
			break;
		}

		if ((err = _cbuffer_init(&new->buffer, SIZE_PAGE)) < 0)
			break;

		hal_spinlockSet(&s->spinlock);
		s->state |= US_ACCEPTING;

		while ((conn = s->connect) == NULL)
			proc_threadWait(&s->queue, &s->spinlock, 0);

		LIST_REMOVE(&s->connect, conn);
		s->state &= ~US_ACCEPTING;

		conn->connect = new;
		new->connect = conn;

		proc_threadWakeup(&conn->queue);
		hal_spinlockClear(&s->spinlock);

		err = new->id;
		unixsock_put(new);
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_bind(unsigned socket, const struct sockaddr *address, socklen_t address_len)
{
	char *path, *name, *dir;
	size_t len;
	int err;
	oid_t odir, dev;
	unixsock_t *s;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->state & US_BOUND) {
			err = -EINVAL;
			break;
		}

		len = hal_strlen(address->sa_data);

		if ((path = vm_kmalloc(len + 1)) == NULL) {
			err = -ENOMEM;
			break;
		}

		do {
			if (s->type == SOCK_DGRAM && _cbuffer_init(&s->buffer, SIZE_PAGE) < 0) {
				err = -ENOMEM;
				break;
			}

			hal_memcpy(path, address->sa_data, len + 1);
			splitname(path, &name, &dir);

			if (proc_lookup(dir, &odir) < 0) {
				err = -ENOTDIR;
				break;
			}

			dev.port = US_PORT;
			dev.id = socket;
			err = proc_create(odir.port, 2 /* otDev */, 0, dev, odir, name, &dev);

			if (!err)
				s->state |= US_BOUND;
		} while (0);

		vm_kfree(path);
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_listen(unsigned socket, int backlog)
{
	unixsock_t *s;
	int err;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->state & US_LISTENING) {
			err = -EADDRINUSE;
			break;
		}

		if (s->type != SOCK_STREAM && s->type != SOCK_SEQPACKET) {
			err = -EOPNOTSUPP;
			break;
		}

		s->state |= US_LISTENING;
		err = EOK;
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_connect(unsigned socket, const struct sockaddr *address, socklen_t address_len)
{
	unixsock_t *s, *remote;
	int err;
	oid_t oid;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->state & US_LISTENING) {
			err = -EADDRINUSE;
			break;
		}

		if (s->connect != NULL) {
			err = -EISCONN;
			break;
		}

		if (proc_lookup(address->sa_data, &oid) < 0) {
			err = -ECONNREFUSED;
			break;
		}

		if (s->type != SOCK_STREAM && s->type != SOCK_SEQPACKET) {
			err = -EOPNOTSUPP;
			break;
		}

		if (oid.port != US_PORT || (remote = unixsock_get(oid.id)) == NULL) {
			err = -ECONNREFUSED;
			break;
		}

		do {
			if (!(remote->state & US_LISTENING)) {
				err = -ECONNREFUSED;
				break;
			}

			if ((err = _cbuffer_init(&s->buffer, SIZE_PAGE)) < 0)
				break;

			hal_spinlockSet(&remote->spinlock);
			LIST_ADD(&remote->connect, s);
			proc_threadWakeup(&remote->queue);
			hal_spinlockClear(&remote->spinlock);

			hal_spinlockSet(&s->spinlock);
			s->state |= US_CONNECTING;

			while (s->connect == NULL)
				proc_threadWait(&s->queue, &s->spinlock, 0);

			s->state &= ~US_CONNECTING;
			hal_spinlockClear(&s->spinlock);

			err = EOK;
		} while (0);

		unixsock_put(remote);
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_getpeername(unsigned socket, struct sockaddr *address, socklen_t *address_len)
{
	return 0;
}


int unix_getsockname(unsigned socket, struct sockaddr *address, socklen_t *address_len)
{
	return 0;
}


int unix_getsockopt(unsigned socket, int level, int optname, void *optval, socklen_t *optlen)
{
	return 0;
}


ssize_t unix_recvfrom(unsigned socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	unixsock_t *s;
	size_t rlen = 0;
	int err = 0;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	for (;;) {
		proc_lockSet(&s->lock);
		if (s->type == SOCK_STREAM) {
			err = _cbuffer_read(&s->buffer, message, length);
		}
		else if (_cbuffer_avail(&s->buffer) > 0) { /* SOCK_DGRAM or SOCK_SEQPACKET */
			_cbuffer_read(&s->buffer, &rlen, sizeof(rlen));
			_cbuffer_read(&s->buffer, message, err = min(length, rlen));

			if (length < rlen)
				_cbuffer_discard(&s->buffer, rlen - length);
		}
		proc_lockClear(&s->lock);

		if (err > 0) {
			hal_spinlockSet(&s->spinlock);
			proc_threadWakeup(&s->writeq);
			hal_spinlockClear(&s->spinlock);

			break;
		}
		else if (flags & MSG_DONTWAIT) {
			err = -EWOULDBLOCK;
			break;
		}

		hal_spinlockSet(&s->spinlock);
		proc_threadWait(&s->queue, &s->spinlock, 0);
		hal_spinlockClear(&s->spinlock);
	}

	unixsock_put(s);
	return err;
}


ssize_t unix_sendto(unsigned socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	unixsock_t *s, *conn;
	int err;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->type == SOCK_DGRAM) {
			conn = s;
		}
		else if (s->connect == NULL && dest_addr != NULL) {
			if ((err = unix_lookupSocket(dest_addr->sa_data)) < 0)
				break;

			if ((conn = unixsock_get(err)) == NULL) {
				err = -ENOTSOCK;
				break;
			}
		}
		else if ((conn = s->connect) == NULL) {
			err = -ENOTCONN;
			break;
		}

		err = 0;
		for (;;) {
			proc_lockSet(&conn->lock);
			if (s->type == SOCK_STREAM) {
				err = _cbuffer_write(&conn->buffer, message, length);
			}
			else if (_cbuffer_free(&conn->buffer) >= length + sizeof(length)) {
				_cbuffer_write(&conn->buffer, &length, sizeof(length));
				_cbuffer_write(&conn->buffer, message, err = length);
				proc_threadWakeup(&conn->queue);
			}
			proc_lockClear(&conn->lock);

			if (err > 0) {
				hal_spinlockSet(&conn->spinlock);
				proc_threadWakeup(&conn->queue);
				hal_spinlockClear(&conn->spinlock);

				break;
			}
			else if (flags & MSG_DONTWAIT) {
				err = -EWOULDBLOCK;
				break;
			}

			hal_spinlockSet(&conn->spinlock);
			proc_threadWait(&conn->writeq, &conn->spinlock, 0);
			hal_spinlockClear(&conn->spinlock);
		}

	} while (0);

	unixsock_put(s);
	return err;
}


int unix_shutdown(unsigned socket, int how)
{
	unixsock_t *s;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	unixsock_put(s);
	unixsock_put(s);
}


int unix_setsockopt(unsigned socket, int level, int optname, const void *optval, socklen_t optlen)
{
	return 0;
}


void unix_sockets_init(void)
{
	lib_rbInit(&unix_common.tree, unixsock_cmp, unixsock_augment);
	proc_lockInit(&unix_common.lock);
}
