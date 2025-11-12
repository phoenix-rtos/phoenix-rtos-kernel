/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module, UNIX sockets
 *
 * Copyright 2018, 2020, 2025 Phoenix Systems
 * Author: Jan Sikorski, Pawel Pisarczyk, Ziemowit Leszczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "proc/proc.h"
#include "lib/lib.h"

#include "posix.h"
#include "posix_private.h"
#include "fdpass.h"


/*
 * FIXME: this module has multiple potential race conditions. For example,
 * in unix_bind(), unix_connect(), and other related functions.
 */

#define US_DEF_BUFFER_SIZE SIZE_PAGE
#define US_MIN_BUFFER_SIZE SIZE_PAGE
#define US_MAX_BUFFER_SIZE 65536U

#define US_BOUND       (1U << 0)
#define US_LISTENING   (1U << 1)
#define US_ACCEPTING   (1U << 2)
#define US_CONNECTING  (1U << 3)
#define US_PEER_CLOSED (1U << 4)

typedef struct _unixsock_t {
	rbnode_t linkage;
	unsigned int id;
	unsigned int lmaxgap;
	unsigned int rmaxgap;

	struct _unixsock_t *next, *prev;

	int refs;

	lock_t lock;
	cbuffer_t buffer;
	size_t buffsz;
	fdpack_t *fdpacks;

	u8 type;
	u8 state;
	u8 nonblock;

	spinlock_t spinlock;

	/* Socket to which this socket is connected to. */
	struct _unixsock_t *remote;

	/* For SOCK_DGRAM: list of sockets connected to this socket. */
	struct _unixsock_t *connected;

	/* For other types: list of sockets requesting a connection. */
	struct _unixsock_t *connecting;

	thread_t *queue;
	thread_t *writeq;
} unixsock_t;


static struct {
	rbtree_t tree;
	lock_t lock;
} unix_common;


static int unixsock_cmp(rbnode_t *n1, rbnode_t *n2)
{
	unsigned int id_diff;
	unixsock_t *r1 = lib_treeof(unixsock_t, linkage, n1);
	unixsock_t *r2 = lib_treeof(unixsock_t, linkage, n2);

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	if (r1->id < r2->id) {
		id_diff = r2->id - r1->id;
		return -1 * (int)(id_diff);
	}
	else {
		id_diff = r1->id - r2->id;
		return (int)(id_diff);
	}
}


static int unixsock_gapcmp(rbnode_t *n1, rbnode_t *n2)
{
	unixsock_t *r1 = lib_treeof(unixsock_t, linkage, n1);
	unixsock_t *r2 = lib_treeof(unixsock_t, linkage, n2);
	rbnode_t *child = NULL;
	int ret = 1;

	/* parasoft-begin-suppress MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	if (r1->lmaxgap > 0U && r1->rmaxgap > 0U) {
		if (r2->id > r1->id) {
			/* parasoft-end-suppress MISRAC2012-DIR_4_1 */
			child = n1->right;
			ret = -1;
		}
		else {
			child = n1->left;
			ret = 1;
		}
	}
	else if (r1->lmaxgap > 0U) {
		child = n1->left;
		ret = 1;
	}
	else if (r1->rmaxgap > 0U) {
		child = n1->right;
		ret = -1;
	}
	else {
		/* No action required */
	}

	if (child == NULL) {
		return 0;
	}

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
			if (it->parent->right == it) {
				break;
			}
		}

		/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
		n->lmaxgap = (n->id <= p->id) ? n->id : n->id - p->id - 1U;
	}
	else {
		l = lib_treeof(unixsock_t, linkage, node->left);
		/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(unixsock_t, linkage, it->parent);
			if (it->parent->left == it) {
				break;
			}
		}

		n->rmaxgap = (n->id >= p->id) ? (unsigned int)-1 - n->id - 1U : p->id - n->id - 1U;
	}
	else {
		r = lib_treeof(unixsock_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(unixsock_t, linkage, it);
		p = lib_treeof(unixsock_t, linkage, it->parent);

		if (it->parent->left == it) {
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		}
		else {
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
		}
	}
}


static unixsock_t *unixsock_alloc(unsigned int *id, unsigned int type, int nonblock)
{
	unixsock_t *r, t;

	*id = 0;
	(void)proc_lockSet(&unix_common.lock);
	if (unix_common.tree.root != NULL) {
		t.id = 0;
		r = lib_treeof(unixsock_t, linkage, lib_rbFindEx(unix_common.tree.root, &t.linkage, unixsock_gapcmp));
		if (r != NULL) {
			if (r->lmaxgap > 0U) {
				*id = r->id - 1U;
			}
			else {
				*id = r->id + 1U;
			}
		}
		else {
			(void)proc_lockClear(&unix_common.lock);
			return NULL;
		}
	}

	r = vm_kmalloc(sizeof(unixsock_t));
	if (r == NULL) {
		(void)proc_lockClear(&unix_common.lock);
		return NULL;
	}

	(void)proc_lockInit(&r->lock, &proc_lockAttrDefault, "unix.socket");

	r->id = *id;
	/* alloc new socket with 2 refs: one ref for the socket's presence in the tree, second
	 * one for handling by the caller before returning the socket to the user (to protect
	 * against accidental socket removal by someone else in the meantime) */
	r->refs = 2;
	r->type = (u8)type;
	r->nonblock = (u8)nonblock;
	r->buffsz = US_DEF_BUFFER_SIZE;
	r->fdpacks = NULL;
	r->remote = NULL;
	r->connected = NULL;
	r->connecting = NULL;
	r->queue = NULL;
	r->writeq = NULL;
	r->state = 0;
	r->next = NULL;
	r->prev = NULL;
	_cbuffer_init(&r->buffer, NULL, 0);
	hal_spinlockCreate(&r->spinlock, "unix socket");

	(void)lib_rbInsert(&unix_common.tree, &r->linkage);
	(void)proc_lockClear(&unix_common.lock);

	return r;
}


static unixsock_t *unixsock_get(unsigned int id)
{
	unixsock_t *r, t;

	t.id = id;

	(void)proc_lockSet(&unix_common.lock);
	r = lib_treeof(unixsock_t, linkage, lib_rbFind(&unix_common.tree, &t.linkage));
	if (r != NULL) {
		r->refs++;
	}
	(void)proc_lockClear(&unix_common.lock);

	return r;
}


static unixsock_t *unixsock_get_remote(unixsock_t *s)
{
	unixsock_t *r;

	(void)proc_lockSet(&unix_common.lock);
	r = s->remote;
	if (r != NULL) {
		r->refs++;
	}
	(void)proc_lockClear(&unix_common.lock);

	return r;
}


static void unixsock_put(unixsock_t *s)
{
	unixsock_t *r;

	(void)proc_lockSet(&unix_common.lock);
	s->refs--;
	if (s->refs <= 0) {
		lib_rbRemove(&unix_common.tree, &s->linkage);

		if (s->remote != NULL) {
			if (s->type == SOCK_DGRAM) {
				LIST_REMOVE(&s->remote->connected, s);
			}
			else {
				s->remote->state |= US_PEER_CLOSED;
				s->remote->remote = NULL;
			}
		}

		if (s->type == SOCK_DGRAM) {
			r = s->connected;
			if (r != NULL) {
				do {
					r->state |= US_PEER_CLOSED;
					r->remote = NULL;
					r = r->next;
				} while (r != s->connected);
			}
		}
		else {
			/* FIXME: handle connecting socket */
		}

		(void)proc_lockClear(&unix_common.lock);

		(void)proc_lockDone(&s->lock);
		hal_spinlockDestroy(&s->spinlock);
		if (s->buffer.data != NULL) {
			vm_kfree(s->buffer.data);
		}
		if (s->fdpacks != NULL) {
			(void)fdpass_discard(&s->fdpacks);
		}
		vm_kfree(s);
		return;
	}
	(void)proc_lockClear(&unix_common.lock);
}


int unix_socket(int domain, unsigned int type, int protocol)
{
	unixsock_t *s;
	unsigned int id;
	int nonblock;

	nonblock = ((type & SOCK_NONBLOCK) != 0U) ? 1 : 0;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

	if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET) {
		return -EPROTOTYPE;
	}

	if (protocol != PF_UNSPEC) {
		return -EPROTONOSUPPORT;
	}

	s = unixsock_alloc(&id, type, nonblock);
	if (s == NULL) {
		return -ENOMEM;
	}
	unixsock_put(s);

	return (int)id;
}


int unix_socketpair(int domain, unsigned int type, int protocol, int sv[2])
{
	unixsock_t *s[2];
	unsigned int id[2];
	void *v[2];
	int nonblock;

	nonblock = ((type & SOCK_NONBLOCK) != 0U) ? 1 : 0;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

	if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET) {
		return -EPROTOTYPE;
	}

	if (protocol != PF_UNSPEC) {
		return -EPROTONOSUPPORT;
	}

	s[0] = unixsock_alloc(&id[0], type, nonblock);
	if (s[0] == NULL) {
		return -ENOMEM;
	}

	s[1] = unixsock_alloc(&id[1], type, nonblock);
	if (s[1] == NULL) {
		unixsock_put(s[0]);
		unixsock_put(s[0]);
		return -ENOMEM;
	}

	v[0] = vm_kmalloc(s[0]->buffsz);
	if (v[0] == NULL) {
		unixsock_put(s[1]);
		unixsock_put(s[1]);
		unixsock_put(s[0]);
		unixsock_put(s[0]);
		return -ENOMEM;
	}

	v[1] = vm_kmalloc(s[1]->buffsz);
	if (v[1] == NULL) {
		vm_kfree(v[0]);
		unixsock_put(s[1]);
		unixsock_put(s[1]);
		unixsock_put(s[0]);
		unixsock_put(s[0]);
		return -ENOMEM;
	}

	_cbuffer_init(&s[0]->buffer, v[0], s[0]->buffsz);
	_cbuffer_init(&s[1]->buffer, v[1], s[1]->buffsz);

	s[0]->remote = s[1];
	s[1]->remote = s[0];

	if (type == SOCK_DGRAM) {
		LIST_ADD(&s[0]->connected, s[1]);
		LIST_ADD(&s[1]->connected, s[0]);
	}

	/* TODO: Consider checking value of id before cast to int */
	sv[0] = (int)id[0];
	sv[1] = (int)id[1];

	unixsock_put(s[1]);
	unixsock_put(s[0]);

	return 0;
}


int unix_accept4(unsigned int socket, struct sockaddr *address, socklen_t *address_len, unsigned int flags)
{
	unixsock_t *s, *r, *new;
	int err;
	unsigned int newid;
	void *v;
	spinlock_ctx_t sc;
	int nonblock;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	nonblock = ((flags & SOCK_NONBLOCK) != 0U) ? 1 : 0;

	do {
		if (s->type != SOCK_STREAM && s->type != SOCK_SEQPACKET) {
			err = -EOPNOTSUPP;
			break;
		}

		if ((s->state & US_LISTENING) == 0U) {
			err = -EINVAL;
			break;
		}

		if (s->nonblock != 0U && s->connecting == NULL) {
			err = -EWOULDBLOCK;
			break;
		}

		new = unixsock_alloc(&newid, s->type, nonblock);
		if (new == NULL) {
			err = -ENOMEM;
			break;
		}

		v = vm_kmalloc(new->buffsz);
		if (v == NULL) {
			unixsock_put(new);
			unixsock_put(new);
			err = -ENOMEM;
			break;
		}

		_cbuffer_init(&new->buffer, v, new->buffsz);

		hal_spinlockSet(&s->spinlock, &sc);
		s->state |= US_ACCEPTING;

		while (s->connecting == NULL) {
			(void)proc_threadWait(&s->queue, &s->spinlock, 0, &sc);
		}
		r = s->connecting;

		LIST_REMOVE(&s->connecting, r);

		s->state &= ~US_ACCEPTING;
		hal_spinlockClear(&s->spinlock, &sc);

		/* FIXME: handle connecting socket removal */

		hal_spinlockSet(&r->spinlock, &sc);

		r->state &= ~(US_PEER_CLOSED | US_CONNECTING);
		r->remote = new;
		new->remote = r;

		(void)proc_threadWakeup(&r->queue);
		hal_spinlockClear(&r->spinlock, &sc);

		err = (int)new->id;
		unixsock_put(new);
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_bind(unsigned int socket, const struct sockaddr *address, socklen_t address_len)
{
	char *path, *name;
	const char *dir;
	int err;
	oid_t odir, dev;
	unixsock_t *s;
	void *v = NULL;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	do {
		if ((s->state & US_BOUND) != 0U) {
			err = -EINVAL;
			break;
		}

		if (address->sa_family != (sa_family_t)AF_UNIX) {
			err = -EINVAL;
			break;
		}

		path = lib_strdup(address->sa_data);
		if (path == NULL) {
			err = -ENOMEM;
			break;
		}

		do {
			lib_splitname(path, &name, &dir);

			if (proc_lookup(dir, NULL, &odir) < 0) {
				err = -ENOTDIR;
				break;
			}

			if (s->type == SOCK_DGRAM) {
				v = vm_kmalloc(s->buffsz);
				if (v == NULL) {
					err = -ENOMEM;
					break;
				}

				_cbuffer_init(&s->buffer, v, s->buffsz);
			}

			dev.port = US_PORT;
			dev.id = socket;
			err = proc_create(odir.port, 2 /* otDev */, S_IFSOCK, dev, odir, name, &dev);

			if (err != 0) {
				if (s->type == SOCK_DGRAM) {
					_cbuffer_init(&s->buffer, NULL, 0);
					vm_kfree(v);
				}
				break;
			}

			s->state |= US_BOUND;
		} while (0);

		vm_kfree(path);
	} while (0);

	unixsock_put(s);
	return err;
}


/* TODO: use backlog */
int unix_listen(unsigned int socket, int backlog)
{
	unixsock_t *s;
	int err;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	do {
		if ((s->state & US_LISTENING) != 0U) {
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


/* TODO: add support for disconnecting and reconnecting a SOCK_DGRAM socket using AF_UNSPEC. */
int unix_connect(unsigned int socket, const struct sockaddr *address, socklen_t address_len)
{
	unixsock_t *s, *r;
	int err;
	oid_t oid;
	void *v;
	spinlock_ctx_t sc;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	do {
		if ((s->state & US_LISTENING) != 0U) {
			err = -EADDRINUSE;
			break;
		}

		if ((s->state & US_CONNECTING) != 0U) {
			err = -EALREADY;
			break;
		}

		if (s->remote != NULL || (s->state & US_PEER_CLOSED) != 0U) {
			err = -EISCONN;
			break;
		}

		if (s->type != SOCK_STREAM && s->type != SOCK_SEQPACKET && s->type != SOCK_DGRAM) {
			err = -EOPNOTSUPP;
			break;
		}

		if (address->sa_family != (sa_family_t)AF_UNIX) {
			err = -EINVAL;
			break;
		}

		err = proc_lookup(address->sa_data, NULL, &oid);
		if (err < 0) {
			err = -ECONNREFUSED;
			break;
		}

		if (oid.port != US_PORT) {
			err = -ECONNREFUSED;
			break;
		}

		/* FIXME: caller may block indefinitely if remote gets closed after successful unixsock_get call */
		r = unixsock_get((unsigned int)oid.id);
		if (r == NULL) {
			err = -ECONNREFUSED;
			break;
		}

		do {
			if (s->type != r->type) {
				err = -EPROTOTYPE;
				break;
			}

			if (s->type == SOCK_DGRAM) {
				hal_spinlockSet(&s->spinlock, &sc);
				s->state &= ~US_PEER_CLOSED;
				s->remote = r;
				hal_spinlockClear(&s->spinlock, &sc);

				hal_spinlockSet(&r->spinlock, &sc);
				LIST_ADD(&r->connected, s);
				hal_spinlockClear(&r->spinlock, &sc);

				err = EOK;
				break;
			}

			if ((r->state & US_LISTENING) == 0U) {
				err = -ECONNREFUSED;
				break;
			}

			v = vm_kmalloc(s->buffsz);
			if (v == NULL) {
				err = -ENOMEM;
				break;
			}

			_cbuffer_init(&s->buffer, v, s->buffsz);

			/* FIXME: handle remote socket removal */

			hal_spinlockSet(&r->spinlock, &sc);
			LIST_ADD(&r->connecting, s);
			(void)proc_threadWakeup(&r->queue);
			hal_spinlockClear(&r->spinlock, &sc);

			hal_spinlockSet(&s->spinlock, &sc);
			s->state |= US_CONNECTING;

			if (s->nonblock != 0U && s->remote == NULL) {
				hal_spinlockClear(&s->spinlock, &sc);
				err = -EINPROGRESS;
				break;
			}

			while (s->remote == NULL) {
				(void)proc_threadWait(&s->queue, &s->spinlock, 0, &sc);
			}

			hal_spinlockClear(&s->spinlock, &sc);

			err = EOK;
		} while (0);

		unixsock_put(r);
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_getpeername(unsigned int socket, struct sockaddr *address, socklen_t *address_len)
{
	return 0;
}


int unix_getsockname(unsigned int socket, struct sockaddr *address, socklen_t *address_len)
{
	return 0;
}


int unix_getsockopt(unsigned int socket, int level, int optname, void *optval, socklen_t *optlen)
{
	unixsock_t *s;
	int err = EOK;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}
	do {
		if (level != SOL_SOCKET) {
			err = -EINVAL;
			break;
		}

		switch ((unsigned int)optname) {
			case SO_RCVBUF:
				if (optval != NULL && *optlen >= sizeof(int)) {
					*((unsigned int *)optval) = s->buffsz;
					*optlen = sizeof(int);
				}
				else {
					err = -EINVAL;
				}
				break;
			case SO_ERROR:
				if (s->remote == NULL && s->nonblock != 0U && (s->state & US_CONNECTING) != 0U) {
					/* non-blocking connect() in progress, not connected yet */
					err = -EINPROGRESS;
				}
				/* TODO: implement default SO_ERROR purpose: read and clear pending socket error info */
				break;

			default:
				err = -ENOPROTOOPT;
				break;
		}
	} while (0);

	unixsock_put(s);
	return err;
}


static ssize_t recv(unsigned int socket, void *buf, size_t len, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len, void *control, socklen_t *controllen)
{
	unixsock_t *s;
	size_t rlen = 0;
	ssize_t err;
	spinlock_ctx_t sc;
	unsigned int peek;

	peek = ((flags & MSG_PEEK) != 0U) ? 1U : 0U;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	do {
		if (s->type != SOCK_DGRAM && s->remote == NULL && (s->state & US_PEER_CLOSED) == 0U) {
			err = -ENOTCONN;
			break;
		}

		err = 0;

		for (;;) {
			(void)proc_lockSet(&s->lock);
			if (s->type == SOCK_STREAM) {
				if (peek != 0U) {
					err = (int)_cbuffer_peek(&s->buffer, buf, len);
				}
				else {
					err = (int)_cbuffer_read(&s->buffer, buf, len);
				}
			}
			else if (_cbuffer_avail(&s->buffer) > 0U) { /* SOCK_DGRAM or SOCK_SEQPACKET */
				/* TODO: handle MSG_PEEK */
				(void)_cbuffer_read(&s->buffer, &rlen, sizeof(rlen));
				(void)_cbuffer_read(&s->buffer, buf, min(len, rlen));
				err = (int)min(len, rlen);

				if (len < rlen) {
					(void)_cbuffer_discard(&s->buffer, rlen - len);
				}
			}
			else {
				/* No action required */
			}
			/* TODO: peek control data */
			if (peek == 0U) {
				if (err > 0 && control != NULL && controllen != NULL && *controllen > 0U) {
					(void)fdpass_unpack(&s->fdpacks, control, controllen);
				}
			}
			(void)proc_lockClear(&s->lock);

			if (err > 0) {
				if (peek == 0U) {
					hal_spinlockSet(&s->spinlock, &sc);
					(void)proc_threadWakeup(&s->writeq);
					hal_spinlockClear(&s->spinlock, &sc);
				}
				break;
			}
			else if (s->type != SOCK_DGRAM && (s->state & US_PEER_CLOSED) != 0U) {
				err = 0; /* EOS */
				break;
			}
			else if (s->nonblock != 0U || (flags & MSG_DONTWAIT) != 0U) {
				err = -EWOULDBLOCK;
				break;
			}
			else {
				/* No action required */
			}

			hal_spinlockSet(&s->spinlock, &sc);
			(void)proc_threadWait(&s->queue, &s->spinlock, 0, &sc);
			hal_spinlockClear(&s->spinlock, &sc);
		}
	} while (0);

	unixsock_put(s);
	return err;
}


/* TODO: a connected SOCK_DGRAM socket should only receive data from its peer. */
static ssize_t send(unsigned int socket, const void *buf, size_t len, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len, fdpack_t *fdpack)
{
	unixsock_t *s, *r;
	ssize_t err;
	oid_t oid;
	spinlock_ctx_t sc;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	do {
		if (s->type == SOCK_DGRAM) {
			if (dest_addr != NULL && dest_len != 0U) {
				if (dest_addr->sa_family != (sa_family_t)AF_UNIX) {
					err = -EINVAL;
					break;
				}

				err = proc_lookup(dest_addr->sa_data, NULL, &oid);
				if (err < 0) {
					err = -ECONNREFUSED;
					break;
				}

				if (oid.port != US_PORT) {
					err = -ECONNREFUSED;
					break;
				}

				r = unixsock_get((unsigned int)oid.id);
				if (r == NULL) {
					err = -ENOTSOCK;
					break;
				}

				if (s->type != r->type) {
					unixsock_put(r);
					err = -EPROTOTYPE;
					break;
				}
			}
			else {
				if ((s->state & US_PEER_CLOSED) != 0U) {
					hal_spinlockSet(&s->spinlock, &sc);
					s->state &= ~US_PEER_CLOSED;
					hal_spinlockClear(&s->spinlock, &sc);
					err = -ECONNREFUSED;
					break;
				}

				r = unixsock_get_remote(s);
				if (r == NULL) {
					err = -ENOTCONN;
					break;
				}
			}
		}
		else {
			if (dest_addr != NULL || dest_len != 0U) {
				err = -EISCONN;
				break;
			}

			if ((s->state & US_PEER_CLOSED) != 0U) {
				(void)posix_tkill(process_getPid(proc_current()->process), 0, SIGPIPE);
				err = -EPIPE;
				break;
			}

			r = unixsock_get_remote(s);
			if (r == NULL) {
				err = -ENOTCONN;
				break;
			}
			else {
				/* No action required */
			}
		}

		err = 0;

		if (len > 0U) {
			for (;;) {
				(void)proc_lockSet(&r->lock);
				if (s->type == SOCK_STREAM) {
					err = (int)_cbuffer_write(&r->buffer, buf, len);
				}
				else if (_cbuffer_free(&r->buffer) >= len + sizeof(len)) { /* SOCK_DGRAM or SOCK_SEQPACKET */
					(void)_cbuffer_write(&r->buffer, &len, sizeof(len));
					(void)_cbuffer_write(&r->buffer, buf, len);
					err = (int)len;
				}
				else if (r->buffsz < len + sizeof(len)) { /* SOCK_DGRAM or SOCK_SEQPACKET */
					err = -EMSGSIZE;
					(void)proc_lockClear(&r->lock);
					break;
				}
				else {
					/* No action required */
				}

				if (err > 0 && fdpack != NULL) {
					LIST_ADD(&r->fdpacks, fdpack);
				}

				(void)proc_lockClear(&r->lock);

				if (err > 0) {
					hal_spinlockSet(&r->spinlock, &sc);
					(void)proc_threadWakeup(&r->queue);
					hal_spinlockClear(&r->spinlock, &sc);

					break;
				}
				else if (s->nonblock != 0U || (flags & MSG_DONTWAIT) != 0U) {
					err = -EWOULDBLOCK;
					break;
				}
				else {
					/* No action required */
				}

				hal_spinlockSet(&r->spinlock, &sc);
				(void)proc_threadWait(&r->writeq, &r->spinlock, 0, &sc);
				hal_spinlockClear(&r->spinlock, &sc);
			}
		}

		unixsock_put(r);
	} while (0);

	unixsock_put(s);
	return err;
}


ssize_t unix_recvfrom(unsigned int socket, void *msg, size_t len, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	return recv(socket, msg, len, flags, src_addr, src_len, NULL, NULL);
}


ssize_t unix_sendto(unsigned int socket, const void *msg, size_t len, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	return send(socket, msg, len, flags, dest_addr, dest_len, NULL);
}


ssize_t unix_recvmsg(unsigned int socket, struct msghdr *msg, unsigned int flags)
{
	ssize_t err;
	void *buf = NULL;
	size_t len = 0;

	/* multiple buffers are not supported */
	if (msg->msg_iovlen > 1) {
		return -EINVAL;
	}

	if (msg->msg_iovlen > 0) {
		buf = msg->msg_iov->iov_base;
		len = msg->msg_iov->iov_len;
	}

	err = recv(socket, buf, len, flags, msg->msg_name, &msg->msg_namelen, msg->msg_control, &msg->msg_controllen);

	if (err >= 0) {
		/* output flags are not supported */
		msg->msg_flags = 0;
	}

	return err;
}


ssize_t unix_sendmsg(unsigned int socket, const struct msghdr *msg, unsigned int flags)
{
	ssize_t err;
	fdpack_t *fdpack = NULL;
	const void *buf = NULL;
	size_t len = 0;

	/* multiple buffers are not supported */
	if (msg->msg_iovlen > 1) {
		return -EINVAL;
	}

	if (msg->msg_controllen > 0U) {
		err = fdpass_pack(&fdpack, msg->msg_control, msg->msg_controllen);
		if (err < 0) {
			return err;
		}
	}

	if (msg->msg_iovlen > 0) {
		buf = msg->msg_iov->iov_base;
		len = msg->msg_iov->iov_len;
	}

	err = send(socket, buf, len, flags, msg->msg_name, msg->msg_namelen, fdpack);

	/* file descriptors are passed only when some bytes have been sent */
	if (fdpack != NULL && err <= 0) {
		(void)fdpass_discard(&fdpack);
	}
	return err;
}


/* TODO: proper shutdown, link, unlink */
int unix_shutdown(unsigned int socket, int how)
{
	unixsock_t *s;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	unixsock_put(s);
	unixsock_put(s);
	return EOK;
}


/* TODO: copy data from old buffer */
static int unix_bufferSetSize(unixsock_t *s, const size_t sz)
{
	void *v[2] = { NULL, NULL };

	if (sz < US_MIN_BUFFER_SIZE || sz > US_MAX_BUFFER_SIZE) {
		return -EINVAL;
	}

	(void)proc_lockSet(&s->lock);

	if (s->buffer.data != NULL) {
		v[0] = vm_kmalloc(sz);
		if (v[0] == NULL) {
			(void)proc_lockClear(&s->lock);
			return -ENOMEM;
		}

		v[1] = s->buffer.data;
		_cbuffer_init(&s->buffer, v[0], sz);
	}

	s->buffsz = sz;

	(void)proc_lockClear(&s->lock);

	if (v[1] != NULL) {
		vm_kfree(v[1]);
	}

	return 0;
}

int unix_setsockopt(unsigned int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	unixsock_t *s;
	int err;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	do {
		if (level != SOL_SOCKET) {
			err = -EINVAL;
			break;
		}

		switch ((unsigned int)optname) {
			case SO_RCVBUF:
				if (optval != NULL && optlen == sizeof(int)) {
					err = unix_bufferSetSize(s, *((const size_t *)optval));
				}
				else {
					err = -EINVAL;
				}
				break;

			default:
				err = -ENOPROTOOPT;
				break;
		}
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_setfl(unsigned int socket, unsigned int flags)
{
	unixsock_t *s;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	s->nonblock = ((flags & O_NONBLOCK) != 0U) ? 1U : 0U;

	unixsock_put(s);
	return 0;
}


int unix_getfl(unsigned int socket)
{
	unixsock_t *s;
	unsigned int flags;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	flags = O_RDWR;
	if (s->nonblock != 0U) {
		flags |= O_NONBLOCK;
	}

	unixsock_put(s);
	return (int)flags;
}


int unix_unlink(unsigned int socket)
{
	/* TODO: broken - socket may be phony */
	return EOK;
}


int unix_close(unsigned int socket)
{
	unixsock_t *s;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	unixsock_put(s);
	unixsock_put(s);
	return EOK;
}


int unix_poll(unsigned int socket, unsigned short events)
{
	unixsock_t *s, *r;
	unsigned int err = 0;

	s = unixsock_get(socket);
	if (s == NULL) {
		err = POLLNVAL;
	}
	else {
		if ((events & (POLLIN | POLLRDNORM | POLLRDBAND)) != 0U) {
			(void)proc_lockSet(&s->lock);
			if (_cbuffer_avail(&s->buffer) > 0U || (s->connecting != NULL && (s->state & US_LISTENING) != 0U)) {
				err |= (unsigned int)events & (POLLIN | POLLRDNORM | POLLRDBAND);
			}
			(void)proc_lockClear(&s->lock);
		}

		if ((events & (POLLOUT | POLLWRNORM | POLLWRBAND)) != 0U) {
			r = unixsock_get_remote(s);
			if (r != NULL) {
				(void)proc_lockSet(&r->lock);
				if (r->type == SOCK_STREAM) {
					if (_cbuffer_free(&r->buffer) > 0U) {
						err |= (unsigned int)events & (POLLOUT | POLLWRNORM | POLLWRBAND);
					}
				}
				else {
					if (_cbuffer_free(&r->buffer) > sizeof(size_t)) { /* SOCK_DGRAM or SOCK_SEQPACKET */
						err |= (unsigned int)events & (POLLOUT | POLLWRNORM | POLLWRBAND);
					}
				}
				(void)proc_lockClear(&r->lock);
				unixsock_put(r);
			}
			else {
				/* FIXME: how to handle unconnected SOCK_DGRAM socket? */
			}
		}

		unixsock_put(s);
	}

	return (int)err;
}


void unix_sockets_init(void)
{
	lib_rbInit(&unix_common.tree, unixsock_cmp, unixsock_augment);
	(void)proc_lockInit(&unix_common.lock, &proc_lockAttrDefault, "unix.common");
}
