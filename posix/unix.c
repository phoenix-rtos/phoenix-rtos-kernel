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
#define US_MAX_BUFFER_SIZE 65536

#define US_BOUND       (1U << 0)
#define US_LISTENING   (1U << 1)
#define US_ACCEPTING   (1U << 2)
#define US_CONNECTING  (1U << 3)
#define US_PEER_CLOSED (1U << 4)

typedef struct _unixsock_t {
	rbnode_t linkage;
	unsigned id;
	unsigned int lmaxgap;
	unsigned int rmaxgap;

	struct _unixsock_t *next, *prev;

	int refs;

	lock_t lock;
	cbuffer_t buffer;
	size_t buffsz;
	fdpack_t *fdpacks;

	__u8 type;
	__u8 state;
	__u8 nonblock;

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


static unixsock_t *unixsock_alloc(unsigned *id, unsigned type, int nonblock)
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

	proc_lockInit(&r->lock, &proc_lockAttrDefault, "unix.socket");

	r->id = *id;
	/* alloc new socket with 2 refs: one ref for the socket's presence in the tree, second
	 * one for handling by the caller before returning the socket to the user (to protect
	 * against accidental socket removal by someone else in the meantime) */
	r->refs = 2;
	r->type = type;
	r->nonblock = nonblock;
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


static unixsock_t *unixsock_get_remote(unixsock_t *s)
{
	unixsock_t *r;

	proc_lockSet(&unix_common.lock);
	r = s->remote;
	if (r != NULL) {
		r->refs++;
	}
	proc_lockClear(&unix_common.lock);

	return r;
}


static void unixsock_put(unixsock_t *s)
{
	unixsock_t *r;

	proc_lockSet(&unix_common.lock);
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

		proc_lockClear(&unix_common.lock);

		proc_lockDone(&s->lock);
		hal_spinlockDestroy(&s->spinlock);
		if (s->buffer.data)
			vm_kfree(s->buffer.data);
		if (s->fdpacks)
			fdpass_discard(&s->fdpacks);
		vm_kfree(s);
		return;
	}
	proc_lockClear(&unix_common.lock);
}


int unix_socket(int domain, unsigned type, int protocol)
{
	unixsock_t *s;
	unsigned id;
	unsigned nonblock;

	nonblock = (type & SOCK_NONBLOCK) != 0;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

	if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET)
		return -EPROTOTYPE;

	if (protocol != PF_UNSPEC)
		return -EPROTONOSUPPORT;

	s = unixsock_alloc(&id, type, nonblock);
	if (s == NULL) {
		return -ENOMEM;
	}
	unixsock_put(s);

	return id;
}


int unix_socketpair(int domain, unsigned type, int protocol, int sv[2])
{
	unixsock_t *s[2];
	unsigned id[2];
	void *v[2];
	int nonblock;

	nonblock = (type & SOCK_NONBLOCK) != 0;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

	if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET)
		return -EPROTOTYPE;

	if (protocol != PF_UNSPEC)
		return -EPROTONOSUPPORT;

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

	if ((v[0] = vm_kmalloc(s[0]->buffsz)) == NULL) {
		unixsock_put(s[1]);
		unixsock_put(s[1]);
		unixsock_put(s[0]);
		unixsock_put(s[0]);
		return -ENOMEM;
	}

	if ((v[1] = vm_kmalloc(s[1]->buffsz)) == NULL) {
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

	sv[0] = id[0];
	sv[1] = id[1];

	unixsock_put(s[1]);
	unixsock_put(s[0]);

	return 0;
}


int unix_accept4(unsigned socket, struct sockaddr *address, socklen_t *address_len, unsigned flags)
{
	unixsock_t *s, *r, *new;
	int err;
	unsigned newid;
	void *v;
	spinlock_ctx_t sc;
	int nonblock;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	nonblock = (flags & SOCK_NONBLOCK) != 0;

	do {
		if (s->type != SOCK_STREAM && s->type != SOCK_SEQPACKET) {
			err = -EOPNOTSUPP;
			break;
		}

		if ((s->state & US_LISTENING) == 0) {
			err = -EINVAL;
			break;
		}

		if (s->nonblock != 0 && s->connecting == NULL) {
			err = -EWOULDBLOCK;
			break;
		}

		new = unixsock_alloc(&newid, s->type, nonblock);
		if (new == NULL) {
			err = -ENOMEM;
			break;
		}

		if ((v = vm_kmalloc(new->buffsz)) == NULL) {
			unixsock_put(new);
			unixsock_put(new);
			err = -ENOMEM;
			break;
		}

		_cbuffer_init(&new->buffer, v, new->buffsz);

		hal_spinlockSet(&s->spinlock, &sc);
		s->state |= US_ACCEPTING;

		while (s->connecting == NULL) {
			proc_threadWait(&s->queue, &s->spinlock, 0, &sc);
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

		proc_threadWakeup(&r->queue);
		hal_spinlockClear(&r->spinlock, &sc);

		err = new->id;
		unixsock_put(new);
	} while (0);

	unixsock_put(s);
	return err;
}


int unix_bind(unsigned socket, const struct sockaddr *address, socklen_t address_len)
{
	char *path, *name, *dir;
	int err;
	oid_t odir, dev;
	unixsock_t *s;
	void *v = NULL;

	if ((s = unixsock_get(socket)) == NULL) {
		return -ENOTSOCK;
	}

	do {
		if ((s->state & US_BOUND) != 0) {
			err = -EINVAL;
			break;
		}

		if (address->sa_family != AF_UNIX) {
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
				if ((v = vm_kmalloc(s->buffsz)) == NULL) {
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


/* TODO: add support for disconnecting and reconnecting a SOCK_DGRAM socket using AF_UNSPEC. */
int unix_connect(unsigned socket, const struct sockaddr *address, socklen_t address_len)
{
	unixsock_t *s, *r;
	int err;
	oid_t oid;
	void *v;
	spinlock_ctx_t sc;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if ((s->state & US_LISTENING) != 0) {
			err = -EADDRINUSE;
			break;
		}

		if ((s->state & US_CONNECTING) != 0) {
			err = -EALREADY;
			break;
		}

		if (s->remote != NULL || (s->state & US_PEER_CLOSED) != 0) {
			err = -EISCONN;
			break;
		}

		if (s->type != SOCK_STREAM && s->type != SOCK_SEQPACKET && s->type != SOCK_DGRAM) {
			err = -EOPNOTSUPP;
			break;
		}

		if (address->sa_family != AF_UNIX) {
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
		r = unixsock_get(oid.id);
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

			if ((r->state & US_LISTENING) == 0) {
				err = -ECONNREFUSED;
				break;
			}

			if ((v = vm_kmalloc(s->buffsz)) == NULL) {
				err = -ENOMEM;
				break;
			}

			_cbuffer_init(&s->buffer, v, s->buffsz);

			/* FIXME: handle remote socket removal */

			hal_spinlockSet(&r->spinlock, &sc);
			LIST_ADD(&r->connecting, s);
			proc_threadWakeup(&r->queue);
			hal_spinlockClear(&r->spinlock, &sc);

			hal_spinlockSet(&s->spinlock, &sc);
			s->state |= US_CONNECTING;

			if (s->nonblock != 0 && s->remote == NULL) {
				hal_spinlockClear(&s->spinlock, &sc);
				err = -EINPROGRESS;
				break;
			}

			while (s->remote == NULL) {
				proc_threadWait(&s->queue, &s->spinlock, 0, &sc);
			}

			hal_spinlockClear(&s->spinlock, &sc);

			err = EOK;
		} while (0);

		unixsock_put(r);
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
	unixsock_t *s;
	int err = EOK;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (level != SOL_SOCKET) {
			err = -EINVAL;
			break;
		}

		switch (optname) {
			case SO_RCVBUF:
				if (optval != NULL && *optlen >= sizeof(int)) {
					*((int *)optval) = s->buffsz;
					*optlen = sizeof(int);
				}
				else {
					err = -EINVAL;
				}
				break;
			case SO_ERROR:
				if (s->remote == NULL && s->nonblock != 0 && (s->state & US_CONNECTING) != 0) {
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


static ssize_t recv(unsigned socket, void *buf, size_t len, unsigned flags, struct sockaddr *src_addr, socklen_t *src_len, void *control, socklen_t *controllen)
{
	unixsock_t *s;
	size_t rlen = 0;
	int err;
	spinlock_ctx_t sc;
	unsigned peek;

	peek = (flags & MSG_PEEK) != 0;

	if ((s = unixsock_get(socket)) == NULL) {
		return -ENOTSOCK;
	}

	do {
		if (s->type != SOCK_DGRAM && s->remote == NULL && (s->state & US_PEER_CLOSED) == 0) {
			err = -ENOTCONN;
			break;
		}

		err = 0;

		for (;;) {
			proc_lockSet(&s->lock);
			if (s->type == SOCK_STREAM) {
				if (peek != 0) {
					err = _cbuffer_peek(&s->buffer, buf, len);
				}
				else {
					err = _cbuffer_read(&s->buffer, buf, len);
				}
			}
			else if (_cbuffer_avail(&s->buffer) > 0) { /* SOCK_DGRAM or SOCK_SEQPACKET */
				/* TODO: handle MSG_PEEK */
				_cbuffer_read(&s->buffer, &rlen, sizeof(rlen));
				_cbuffer_read(&s->buffer, buf, err = min(len, rlen));

				if (len < rlen) {
					_cbuffer_discard(&s->buffer, rlen - len);
				}
			}
			/* TODO: peek control data */
			if (peek == 0) {
				if (err > 0 && control != NULL && controllen != NULL && *controllen > 0) {
					fdpass_unpack(&s->fdpacks, control, controllen);
				}
			}
			proc_lockClear(&s->lock);

			if (err > 0) {
				if (peek == 0) {
					hal_spinlockSet(&s->spinlock, &sc);
					proc_threadWakeup(&s->writeq);
					hal_spinlockClear(&s->spinlock, &sc);
				}
				break;
			}
			else if (s->type != SOCK_DGRAM && (s->state & US_PEER_CLOSED) != 0) {
				err = 0; /* EOS */
				break;
			}
			else if (s->nonblock != 0 || (flags & MSG_DONTWAIT) != 0) {
				err = -EWOULDBLOCK;
				break;
			}

			hal_spinlockSet(&s->spinlock, &sc);
			proc_threadWait(&s->queue, &s->spinlock, 0, &sc);
			hal_spinlockClear(&s->spinlock, &sc);
		}
	} while (0);

	unixsock_put(s);
	return err;
}


/* TODO: a connected SOCK_DGRAM socket should only receive data from its peer. */
static ssize_t send(unsigned socket, const void *buf, size_t len, unsigned flags, const struct sockaddr *dest_addr, socklen_t dest_len, fdpack_t *fdpack)
{
	unixsock_t *s, *r;
	int err;
	oid_t oid;
	spinlock_ctx_t sc;

	s = unixsock_get(socket);
	if (s == NULL) {
		return -ENOTSOCK;
	}

	do {
		if (s->type == SOCK_DGRAM) {
			if (dest_addr != NULL && dest_len != 0) {
				if (dest_addr->sa_family != AF_UNIX) {
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

				r = unixsock_get(oid.id);
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
				if ((s->state & US_PEER_CLOSED) != 0) {
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
			if (dest_addr != NULL || dest_len != 0) {
				err = -EISCONN;
				break;
			}

			if (s->state & US_PEER_CLOSED) {
				posix_tkill(process_getPid(proc_current()->process), 0, SIGPIPE);
				err = -EPIPE;
				break;
			}

			r = unixsock_get_remote(s);
			if (r == NULL) {
				err = -ENOTCONN;
				break;
			}
		}

		err = 0;

		if (len > 0) {
			for (;;) {
				proc_lockSet(&r->lock);
				if (s->type == SOCK_STREAM) {
					err = _cbuffer_write(&r->buffer, buf, len);
				}
				else if (_cbuffer_free(&r->buffer) >= len + sizeof(len)) { /* SOCK_DGRAM or SOCK_SEQPACKET */
					_cbuffer_write(&r->buffer, &len, sizeof(len));
					_cbuffer_write(&r->buffer, buf, err = len);
				}
				else if (r->buffsz < len + sizeof(len)) { /* SOCK_DGRAM or SOCK_SEQPACKET */
					err = -EMSGSIZE;
					proc_lockClear(&r->lock);
					break;
				}

				if (err > 0 && fdpack != NULL) {
					LIST_ADD(&r->fdpacks, fdpack);
				}
				proc_lockClear(&r->lock);

				if (err > 0) {
					hal_spinlockSet(&r->spinlock, &sc);
					proc_threadWakeup(&r->queue);
					hal_spinlockClear(&r->spinlock, &sc);

					break;
				}
				else if (s->nonblock != 0 || (flags & MSG_DONTWAIT) != 0) {
					err = -EWOULDBLOCK;
					break;
				}

				hal_spinlockSet(&r->spinlock, &sc);
				proc_threadWait(&r->writeq, &r->spinlock, 0, &sc);
				hal_spinlockClear(&r->spinlock, &sc);
			}
		}

		unixsock_put(r);
	} while (0);

	unixsock_put(s);
	return err;
}


ssize_t unix_recvfrom(unsigned socket, void *msg, size_t len, unsigned flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	return recv(socket, msg, len, flags, src_addr, src_len, NULL, NULL);
}


ssize_t unix_sendto(unsigned socket, const void *msg, size_t len, unsigned flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	return send(socket, msg, len, flags, dest_addr, dest_len, NULL);
}


ssize_t unix_recvmsg(unsigned socket, struct msghdr *msg, unsigned flags)
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


ssize_t unix_sendmsg(unsigned socket, const struct msghdr *msg, unsigned flags)
{
	ssize_t err;
	fdpack_t *fdpack = NULL;
	const void *buf = NULL;
	size_t len = 0;

	/* multiple buffers are not supported */
	if (msg->msg_iovlen > 1) {
		return -EINVAL;
	}

	if (msg->msg_controllen > 0) {
		if ((err = fdpass_pack(&fdpack, msg->msg_control, msg->msg_controllen)) < 0) {
			return err;
		}
	}

	if (msg->msg_iovlen > 0) {
		buf = msg->msg_iov->iov_base;
		len = msg->msg_iov->iov_len;
	}

	err = send(socket, buf, len, flags, msg->msg_name, msg->msg_namelen, fdpack);

	/* file descriptors are passed only when some bytes have been sent */
	if (fdpack != NULL && err <= 0)
		fdpass_discard(&fdpack);

	return err;
}


/* TODO: proper shutdown, link, unlink */
int unix_shutdown(unsigned socket, int how)
{
	unixsock_t *s;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	unixsock_put(s);
	unixsock_put(s);
	return EOK;
}


/* TODO: copy data from old buffer */
static int unix_bufferSetSize(unixsock_t *s, int sz)
{
	void *v[2] = { NULL, NULL };

	if (sz < US_MIN_BUFFER_SIZE || sz > US_MAX_BUFFER_SIZE) {
		return -EINVAL;
	}

	proc_lockSet(&s->lock);

	if (s->buffer.data != NULL) {
		v[0] = vm_kmalloc(sz);
		if (v[0] == NULL) {
			proc_lockClear(&s->lock);
			return -ENOMEM;
		}

		v[1] = s->buffer.data;
		_cbuffer_init(&s->buffer, v[0], sz);
	}

	s->buffsz = sz;

	proc_lockClear(&s->lock);

	if (v[1] != NULL) {
		vm_kfree(v[1]);
	}

	return 0;
}

int unix_setsockopt(unsigned socket, int level, int optname, const void *optval, socklen_t optlen)
{
	unixsock_t *s;
	int err;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (level != SOL_SOCKET) {
			err = -EINVAL;
			break;
		}

		switch (optname) {
			case SO_RCVBUF:
				if (optval != NULL && optlen == sizeof(int)) {
					err = unix_bufferSetSize(s, *((int *)optval));
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


int unix_setfl(unsigned socket, unsigned flags)
{
	unixsock_t *s;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	s->nonblock = (flags & O_NONBLOCK) != 0;

	unixsock_put(s);
	return 0;
}


int unix_getfl(unsigned socket)
{
	unixsock_t *s;
	unsigned flags;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	flags = O_RDWR;
	if (s->nonblock != 0) {
		flags |= O_NONBLOCK;
	}

	unixsock_put(s);
	return flags;
}


int unix_unlink(unsigned socket)
{
	/* TODO: broken - socket may be phony */
	return EOK;
}


int unix_close(unsigned socket)
{
	unixsock_t *s;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	unixsock_put(s);
	unixsock_put(s);
	return EOK;
}


int unix_poll(unsigned socket, unsigned short events)
{
	unixsock_t *s, *r;
	unsigned err = 0;

	if ((s = unixsock_get(socket)) == NULL) {
		err = POLLNVAL;
	}
	else {
		if (events & (POLLIN | POLLRDNORM | POLLRDBAND)) {
			proc_lockSet(&s->lock);
			if (_cbuffer_avail(&s->buffer) > 0 || (s->connecting != NULL && (s->state & US_LISTENING) != 0)) {
				err |= events & (POLLIN | POLLRDNORM | POLLRDBAND);
			}
			proc_lockClear(&s->lock);
		}

		if (events & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
			r = unixsock_get_remote(s);
			if (r != NULL) {
				proc_lockSet(&r->lock);
				if (r->type == SOCK_STREAM) {
					if (_cbuffer_free(&r->buffer) > 0) {
						err |= events & (POLLOUT | POLLWRNORM | POLLWRBAND);
					}
				}
				else {
					if (_cbuffer_free(&r->buffer) > sizeof(size_t)) { /* SOCK_DGRAM or SOCK_SEQPACKET */
						err |= events & (POLLOUT | POLLWRNORM | POLLWRBAND);
					}
				}
				proc_lockClear(&r->lock);
				unixsock_put(r);
			}
			else {
				/* FIXME: how to handle unconnected SOCK_DGRAM socket? */
			}
		}

		unixsock_put(s);
	}

	return err;
}


void unix_sockets_init(void)
{
	lib_rbInit(&unix_common.tree, unixsock_cmp, unixsock_augment);
	proc_lockInit(&unix_common.lock, &proc_lockAttrDefault, "unix.common");
}
