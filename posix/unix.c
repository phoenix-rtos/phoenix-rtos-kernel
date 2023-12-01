/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module, UNIX sockets
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Jan Sikorski, Pawel Pisarczyk
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


#define US_DEF_BUFFER_SIZE SIZE_PAGE
#define US_MIN_BUFFER_SIZE SIZE_PAGE
#define US_MAX_BUFFER_SIZE 65536

#define US_BOUND       (1 << 0)
#define US_LISTENING   (1 << 1)
#define US_ACCEPTING   (1 << 2)
#define US_CONNECTING  (1 << 3)
#define US_PEER_CLOSED (1 << 4)

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

	char type;
	char state;
	char nonblock;

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


static unixsock_t *unixsock_alloc(unsigned *id, int type, int nonblock)
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

	proc_lockInit(&r->lock, "unix.socket");

	r->id = *id;
	r->refs = 1;
	r->type = type;
	r->nonblock = nonblock;
	r->buffsz = US_DEF_BUFFER_SIZE;
	r->fdpacks = NULL;
	r->connect = NULL;
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


static unixsock_t *unixsock_get_connected(unixsock_t *s)
{
	unixsock_t *r;

	proc_lockSet(&unix_common.lock);
	r = s->connect;
	if (r != NULL)
		r->refs++;
	proc_lockClear(&unix_common.lock);

	return r;
}


static void unixsock_put(unixsock_t *s)
{
	proc_lockSet(&unix_common.lock);
	if (--s->refs <= 0) {
		lib_rbRemove(&unix_common.tree, &s->linkage);

		/* FIXME: handle connecting socket */

		if (s->connect != NULL) {
			s->connect->state |= US_PEER_CLOSED;
			s->connect->connect = NULL;
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


int unix_lookupSocket(const char *path)
{
	int err;
	oid_t oid;

	if ((err = proc_lookup(path, NULL, &oid)) < 0)
		return err;

	if (oid.port != US_PORT)
		return -ENOTSOCK;

	return (unsigned)oid.id;
}


int unix_socket(int domain, int type, int protocol)
{
	unixsock_t *s;
	unsigned id;
	int nonblock;

	nonblock = (type & SOCK_NONBLOCK) != 0;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

	if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET)
		return -EPROTOTYPE;

	if (protocol != PF_UNSPEC)
		return -EPROTONOSUPPORT;

	if ((s = unixsock_alloc(&id, type, nonblock)) == NULL)
		return -ENOMEM;

	return id;
}


int unix_socketpair(int domain, int type, int protocol, int sv[2])
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

	if ((s[0] = unixsock_alloc(&id[0], type, nonblock)) == NULL)
		return -ENOMEM;

	if ((s[1] = unixsock_alloc(&id[1], type, nonblock)) == NULL) {
		unixsock_put(s[0]);
		return -ENOMEM;
	}

	if ((v[0] = vm_kmalloc(s[0]->buffsz)) == NULL) {
		unixsock_put(s[1]);
		unixsock_put(s[0]);
		return -ENOMEM;
	}

	if ((v[1] = vm_kmalloc(s[1]->buffsz)) == NULL) {
		vm_kfree(v[0]);
		unixsock_put(s[1]);
		unixsock_put(s[0]);
		return -ENOMEM;
	}

	_cbuffer_init(&s[0]->buffer, v[0], s[0]->buffsz);
	_cbuffer_init(&s[1]->buffer, v[1], s[1]->buffsz);

	s[0]->connect = s[1];
	s[1]->connect = s[0];

	sv[0] = id[0];
	sv[1] = id[1];

	return 0;
}


/* TODO: nonblocking accept */
int unix_accept4(unsigned socket, struct sockaddr *address, socklen_t *address_len, int flags)
{
	unixsock_t *s, *conn, *new;
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

		if (!(s->state & US_LISTENING)) {
			err = -EINVAL;
			break;
		}

		if ((new = unixsock_alloc(&newid, s->type, nonblock)) == NULL) {
			err = -ENOMEM;
			break;
		}

		if ((v = vm_kmalloc(new->buffsz)) == NULL) {
			unixsock_put(new);
			err = -ENOMEM;
			break;
		}

		_cbuffer_init(&new->buffer, v, new->buffsz);

		hal_spinlockSet(&s->spinlock, &sc);
		s->state |= US_ACCEPTING;

		while ((conn = s->connect) == NULL)
			proc_threadWait(&s->queue, &s->spinlock, 0, &sc);

		LIST_REMOVE(&s->connect, conn);
		s->state &= ~US_ACCEPTING;

		/* FIXME: handle connecting socket removal */

		conn->state &= ~US_PEER_CLOSED;
		conn->connect = new;
		new->connect = conn;

		proc_threadWakeup(&conn->queue);
		hal_spinlockClear(&s->spinlock, &sc);

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

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->state & US_BOUND) {
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

			if (err) {
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


/* TODO: nonblocking connect */
/* TODO: SOCK_DGRAM support */
int unix_connect(unsigned socket, const struct sockaddr *address, socklen_t address_len)
{
	unixsock_t *s, *remote;
	int err;
	oid_t oid;
	void *v;
	spinlock_ctx_t sc;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->state & US_LISTENING) {
			err = -EADDRINUSE;
			break;
		}

		if (s->connect != NULL || (s->type != SOCK_DGRAM && (s->state & US_PEER_CLOSED))) {
			err = -EISCONN;
			break;
		}

		if (proc_lookup(address->sa_data, NULL, &oid) < 0) {
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

			if ((v = vm_kmalloc(s->buffsz)) == NULL) {
				err = -ENOMEM;
				break;
			}

			_cbuffer_init(&s->buffer, v, s->buffsz);

			/* FIXME: handle remote socket removal */

			hal_spinlockSet(&remote->spinlock, &sc);
			LIST_ADD(&remote->connect, s);
			proc_threadWakeup(&remote->queue);
			hal_spinlockClear(&remote->spinlock, &sc);

			hal_spinlockSet(&s->spinlock, &sc);
			s->state |= US_CONNECTING;

			while (s->connect == NULL)
				proc_threadWait(&s->queue, &s->spinlock, 0, &sc);

			s->state &= ~US_CONNECTING;
			hal_spinlockClear(&s->spinlock, &sc);

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

			default:
				err = -ENOPROTOOPT;
				break;
		}
	} while (0);

	unixsock_put(s);
	return err;
}


static ssize_t recv(unsigned socket, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *src_len, void *control, socklen_t *controllen)
{
	unixsock_t *s;
	size_t rlen = 0;
	int err;
	spinlock_ctx_t sc;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->type != SOCK_DGRAM && !s->connect && !(s->state & US_PEER_CLOSED)) {
			err = -ENOTCONN;
			break;
		}

		err = 0;

		for (;;) {
			proc_lockSet(&s->lock);
			if (s->type == SOCK_STREAM) {
				err = _cbuffer_read(&s->buffer, buf, len);
			}
			else if (_cbuffer_avail(&s->buffer) > 0) { /* SOCK_DGRAM or SOCK_SEQPACKET */
				_cbuffer_read(&s->buffer, &rlen, sizeof(rlen));
				_cbuffer_read(&s->buffer, buf, err = min(len, rlen));

				if (len < rlen)
					_cbuffer_discard(&s->buffer, rlen - len);
			}
			if (err > 0 && control && controllen && *controllen > 0)
				fdpass_unpack(&s->fdpacks, control, controllen);
			proc_lockClear(&s->lock);

			if (err > 0) {
				hal_spinlockSet(&s->spinlock, &sc);
				proc_threadWakeup(&s->writeq);
				hal_spinlockClear(&s->spinlock, &sc);

				break;
			}
			else if (s->type != SOCK_DGRAM && (s->state & US_PEER_CLOSED)) {
				err = 0; /* EOS */
				break;
			}
			else if (s->nonblock || (flags & MSG_DONTWAIT)) {
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


static ssize_t send(unsigned socket, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t dest_len, fdpack_t *fdpack)
{
	unixsock_t *s, *conn;
	int err;
	spinlock_ctx_t sc;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	do {
		if (s->type == SOCK_DGRAM) {
			if (dest_addr && dest_len != 0) {
				if ((err = unix_lookupSocket(dest_addr->sa_data)) < 0)
					break;

				if ((conn = unixsock_get(err)) == NULL) {
					err = -ENOTSOCK;
					break;
				}
			}
			else if (s->state & US_PEER_CLOSED) {
				err = -ECONNREFUSED;
				break;
			}
			else if ((conn = unixsock_get_connected(s)) == NULL) {
				err = -ENOTCONN;
				break;
			}
		}
		else {
			if (dest_addr || dest_len != 0) {
				err = -EISCONN;
				break;
			}
			else if (s->state & US_PEER_CLOSED) {
				posix_tkill(process_getPid(proc_current()->process), 0, SIGPIPE);
				err = -EPIPE;
				break;
			}
			else if ((conn = unixsock_get_connected(s)) == NULL) {
				err = -ENOTCONN;
				break;
			}
		}

		err = 0;

		if (len > 0) {
			for (;;) {
				proc_lockSet(&conn->lock);
				if (s->type == SOCK_STREAM) {
					err = _cbuffer_write(&conn->buffer, buf, len);
				}
				else if (_cbuffer_free(&conn->buffer) >= len + sizeof(len)) { /* SOCK_DGRAM or SOCK_SEQPACKET */
					_cbuffer_write(&conn->buffer, &len, sizeof(len));
					_cbuffer_write(&conn->buffer, buf, err = len);
				}
				if (err > 0 && fdpack)
					LIST_ADD(&conn->fdpacks, fdpack);
				proc_lockClear(&conn->lock);

				if (err > 0) {
					hal_spinlockSet(&conn->spinlock, &sc);
					proc_threadWakeup(&conn->queue);
					hal_spinlockClear(&conn->spinlock, &sc);

					break;
				}
				else if (s->nonblock || (flags & MSG_DONTWAIT)) {
					err = -EWOULDBLOCK;
					break;
				}

				hal_spinlockSet(&conn->spinlock, &sc);
				proc_threadWait(&conn->writeq, &conn->spinlock, 0, &sc);
				hal_spinlockClear(&conn->spinlock, &sc);
			}
		}

		unixsock_put(conn);
	} while (0);

	unixsock_put(s);
	return err;
}


ssize_t unix_recvfrom(unsigned socket, void *msg, size_t len, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	return recv(socket, msg, len, flags, src_addr, src_len, NULL, NULL);
}


ssize_t unix_sendto(unsigned socket, const void *msg, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	return send(socket, msg, len, flags, dest_addr, dest_len, NULL);
}


ssize_t unix_recvmsg(unsigned socket, struct msghdr *msg, int flags)
{
	ssize_t err;
	void *buf = NULL;
	size_t len = 0;

	/* multiple buffers are not supported */
	if (msg->msg_iovlen > 1)
		return -EINVAL;

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


ssize_t unix_sendmsg(unsigned socket, const struct msghdr *msg, int flags)
{
	ssize_t err;
	fdpack_t *fdpack = NULL;
	const void *buf = NULL;
	size_t len = 0;

	/* multiple buffers are not supported */
	if (msg->msg_iovlen > 1)
		return -EINVAL;

	if (msg->msg_controllen > 0) {
		if ((err = fdpass_pack(&fdpack, msg->msg_control, msg->msg_controllen)) < 0)
			return err;
	}

	if (msg->msg_iovlen > 0) {
		buf = msg->msg_iov->iov_base;
		len = msg->msg_iov->iov_len;
	}

	err = send(socket, buf, len, flags, msg->msg_name, msg->msg_namelen, fdpack);

	/* file descriptors are passed only when some bytes have been sent */
	if (fdpack && err <= 0)
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


int unix_setfl(unsigned socket, int flags)
{
	unixsock_t *s;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	s->nonblock = (flags & O_NONBLOCK) != 0;

	unixsock_put(s);
	return flags;
}


int unix_getfl(unsigned socket)
{
	unixsock_t *s;
	int flags;

	if ((s = unixsock_get(socket)) == NULL)
		return -ENOTSOCK;

	flags = s->nonblock ? O_NONBLOCK : 0;

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


int unix_poll(unsigned socket, short events)
{
	unixsock_t *s, *conn;
	int err = 0;

	if ((s = unixsock_get(socket)) == NULL) {
		err = POLLNVAL;
	}
	else {
		if (events & (POLLIN | POLLRDNORM | POLLRDBAND)) {
			proc_lockSet(&s->lock);
			if (_cbuffer_avail(&s->buffer) > 0)
				err |= events & (POLLIN | POLLRDNORM | POLLRDBAND);
			proc_lockClear(&s->lock);
		}

		if (events & (POLLOUT | POLLRDNORM | POLLRDBAND)) {
			if ((conn = unixsock_get_connected(s)) != NULL) {
				proc_lockSet(&conn->lock);
				if (conn->type == SOCK_STREAM) {
					if (_cbuffer_free(&conn->buffer) > 0)
						err |= events & (POLLOUT | POLLRDNORM | POLLRDBAND);
				}
				else {
					if (_cbuffer_free(&conn->buffer) > sizeof(size_t)) /* SOCK_DGRAM or SOCK_SEQPACKET */
						err |= events & (POLLOUT | POLLRDNORM | POLLRDBAND);
				}
				proc_lockClear(&conn->lock);
				unixsock_put(conn);
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
	proc_lockInit(&unix_common.lock, "unix.common");
}
