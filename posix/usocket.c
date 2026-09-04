/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module, UNIX sockets
 *
 * Copyright 2018, 2020, 2025, 2026 Phoenix Systems
 * Author: Jan Sikorski, Pawel Pisarczyk, Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "include/errno.h"
#include "include/sockdefs.h"
#include "proc/proc.h"
#include "lib/lib.h"
#include "vm/vm.h"

#include "posix.h"
#include "posix_private.h"
#include "fdpass.h"
#include "uchannel.h"
#include "usocket.h"


/*
 * A UNIX socket is an endpoint (usocket_t) plus, once it is connected, two
 * data channels (uchannel_t): one it reads from and one it writes to. Everything
 * the two ends of a connection have to tell each other is state of the shared
 * channel, so an endpoint never dereferences its peer - which is what makes
 * the locking simple:
 *
 *   - usocket_t.lock protects every mutable field of the endpoint,
 *   - uchannel_t.lock protects the ring buffer, the queued descriptors and the
 *     half-close flags of one direction,
 *   - usocket_common.lock protects the tree of named sockets and the `named`
 *     flag, unnamed sockets (socketpair(), accept()) are in no global
 *     structure at all,
 *   - both objects are reference counted with atomics.
 *
 * Locking rule: a thread never holds more than one of these locks at a time.
 * Two objects are always visited one after the other, with a reference held
 * on the second one, so there is no lock order to get wrong and no deadlock
 * to reason about. Blocking IPC (proc_lookup(), proc_create()) and every call
 * into the file descriptor table (fdpass_*) happen with no lock held.
 *
 * Waiting is done with proc_lockWait() on the lock that owns the predicate,
 * which closes the window between testing a condition and going to sleep.
 * Every state change broadcasts the queue of the predicate it affects while
 * still holding that lock. The waits are interruptible - proc_lockWait() does
 * not reacquire the lock when it reports -EINTR.
 */

#define USOCKET_DEF_BUFFER_SIZE SIZE_PAGE
#define USOCKET_MIN_BUFFER_SIZE SIZE_PAGE
#define USOCKET_MAX_BUFFER_SIZE 65536U

/*
 * Upper bound on the pending connection queue of a listening socket. Each
 * entry is a reference to a connector that has already allocated its own
 * receive buffer, so the queue itself only has to be kept from growing
 * without bound.
 */
#define USOCKET_MAX_BACKLOG 32U

/* Socket flags */
#define USOCKET_NONBLOCK (1U << 0)
#define USOCKET_SHUT_RD  (1U << 1)
#define USOCKET_SHUT_WR  (1U << 2)


/* Socket state */
enum {
	usocketUnconnected = 0, /* not connected (whether named or not) */
	usocketBinding,         /* bind() in progress (exclusive, no lock held during IPC) */
	usocketListening,       /* listen() done, connectors queue up in `pending` */
	usocketConnecting,      /* queued on a listener, waiting for accept() */
	usocketConnected,       /* connected, both data channels are in place */
	usocketAborted,         /* connect() refused or interrupted, `err` says why */
	usocketClosed           /* close() done, the channels are dropped and no syscall can see it */
};


struct _usocket_t {
	idnode_t linkage;               /* tree of named sockets, valid while `named` */
	struct _usocket_t *next, *prev; /* linkage in a listener's pending list */

	int refs; /* atomic */
	u8 type;  /* immutable */
	u8 named; /* protected by usocket_common.lock */

	lock_t lock;
	u8 state;
	u8 flags;
	int err;        /* pending SO_ERROR, positive errno */
	size_t rcvbuf;  /* size of the channels created for this socket */
	uchannel_t *rx; /* counted */
	uchannel_t *tx; /* counted */

	struct _usocket_t *pending; /* connectors waiting to be accepted, counted */
	u8 pendingCnt;
	u8 backlog;

	thread_t *acceptq; /* accept(): pending != NULL or no longer listening */
	thread_t *connq;   /* connect(): state != usocketConnecting */
};


static struct {
	idtree_t tree;
	lock_t lock;
	int nextId;
} usocket_common;


static int usocket_isFramed(const usocket_t *s)
{
	return (s->type != SOCK_STREAM) ? 1 : 0;
}


static usocket_t *usocket_alloc(unsigned int type, int nonblock)
{
	usocket_t *s;

	s = vm_kmalloc(sizeof(usocket_t));
	if (s == NULL) {
		return NULL;
	}

	if (proc_lockInit(&s->lock, &proc_lockAttrDefault, "unix.socket") < 0) {
		vm_kfree(s);
		return NULL;
	}

	s->linkage.id = -1;
	s->next = NULL;
	s->prev = NULL;
	s->refs = 1;
	s->type = (u8)type;
	s->named = 0;
	s->state = (u8)usocketUnconnected;
	s->flags = (nonblock != 0) ? (u8)USOCKET_NONBLOCK : 0U;
	s->err = 0;
	s->rcvbuf = USOCKET_DEF_BUFFER_SIZE;
	s->rx = NULL;
	s->tx = NULL;
	s->pending = NULL;
	s->pendingCnt = 0;
	s->backlog = 0;
	s->acceptq = NULL;
	s->connq = NULL;

	return s;
}


static usocket_t *usocket_ref(usocket_t *s)
{
	if (s != NULL) {
		(void)lib_atomicIncrement(&s->refs);
	}

	return s;
}


static void usocket_put(usocket_t *s)
{
	if (s == NULL) {
		return;
	}

	if (lib_atomicDecrement(&s->refs) != 0) {
		return;
	}

	/*
	 * The last reference is gone so the socket is in no tree, no descriptor and
	 * no pending list, so nothing can reach it and no lock is needed.
	 */
	uchannel_put(s->rx);
	uchannel_put(s->tx);
	(void)proc_lockDone(&s->lock);
	vm_kfree(s);
}


/*
 * Looks a named socket up and takes a reference. A socket present in the tree
 * always holds one reference on behalf of the tree, and that reference is
 * dropped only after the socket has been removed, so this can never resurrect
 * a socket that is being destroyed.
 */
static usocket_t *usocket_get(id_t id)
{
	usocket_t *s;

	if ((id == 0U) || (id > (id_t)MAX_ID)) {
		return NULL;
	}

	(void)proc_lockSet(&usocket_common.lock);

	s = lib_treeof(usocket_t, linkage, lib_idtreeFind(&usocket_common.tree, (int)id));
	(void)usocket_ref(s);

	(void)proc_lockClear(&usocket_common.lock);

	return s;
}


/*
 * Allocates a name (an id) for the socket. Ids are handed out in increasing
 * order rather than reusing the lowest free one, so that a socket file left
 * behind by a closed socket cannot start naming an unrelated one - until the
 * counter wraps after MAX_ID names, from which point on ids are reused and a
 * stale socket file can name a new socket again. Returns the id, or a negative
 * error. `named` and the tree are both protected by usocket_common.lock, so a
 * socket can never end up with two names.
 */
static int usocket_nameAlloc(usocket_t *s)
{
	int id;

	(void)proc_lockSet(&usocket_common.lock);

	if (s->named != 0U) {
		(void)proc_lockClear(&usocket_common.lock);
		return -EINVAL;
	}

	id = lib_idtreeAlloc(&usocket_common.tree, &s->linkage, usocket_common.nextId);
	if (id < 0) {
		/* the id wraps around */
		usocket_common.nextId = 1;
		id = lib_idtreeAlloc(&usocket_common.tree, &s->linkage, usocket_common.nextId);
	}

	if (id >= 0) {
		usocket_common.nextId = (id < (int)MAX_ID) ? (id + 1) : 1;
		s->named = 1;
		(void)usocket_ref(s);
	}
	else {
		id = -ENFILE;
	}

	(void)proc_lockClear(&usocket_common.lock);

	return id;
}


/*
 * Detaches the socket from its name. Returns 1 if the tree reference has to be
 * dropped by the caller.
 */
static int usocket_nameFree(usocket_t *s)
{
	int named;

	(void)proc_lockSet(&usocket_common.lock);

	named = (s->named != 0U) ? 1 : 0;
	if (named != 0) {
		s->named = 0;
		lib_idtreeRemove(&usocket_common.tree, &s->linkage);
	}

	(void)proc_lockClear(&usocket_common.lock);

	return named;
}


static unsigned int usocket_opFlags(const usocket_t *s, unsigned int flags)
{
	unsigned int op = 0;

	if (((s->flags & USOCKET_NONBLOCK) != 0U) || ((flags & MSG_DONTWAIT) != 0U)) {
		op |= UCHANNEL_OP_NONBLOCK;
	}

	if ((flags & MSG_PEEK) != 0U) {
		op |= UCHANNEL_OP_PEEK;
	}

	return op;
}


/*
 * Creates the receive channel of a datagram socket. Such a socket can only be
 * reached once bind() has given it a name, so the channel is created there
 * rather than for every socket - and on the first recv(), which has to block on
 * an unbound socket rather than fail. Does nothing if the channel is already in
 * place, the loser of a race frees its own allocation.
 */
static int usocket_rxCreate(usocket_t *s)
{
	uchannel_t *rx;
	size_t size;
	int hasRx;

	(void)proc_lockSet(&s->lock);
	size = s->rcvbuf;
	hasRx = (s->rx != NULL) ? 1 : 0;
	(void)proc_lockClear(&s->lock);

	if (hasRx != 0) {
		return EOK;
	}

	rx = uchannel_alloc(size, 1);
	if (rx == NULL) {
		return -ENOMEM;
	}

	(void)proc_lockSet(&s->lock);
	if ((s->rx == NULL) && (s->state != (u8)usocketClosed)) {
		s->rx = rx;
		rx = NULL;
	}
	(void)proc_lockClear(&s->lock);

	uchannel_put(rx);

	return EOK;
}


int usocket_socket(int domain, unsigned int type, int protocol, usocket_t **s)
{
	usocket_t *ns;
	int nonblock;

	nonblock = ((type & SOCK_NONBLOCK) != 0U) ? 1 : 0;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

	if ((type != SOCK_STREAM) && (type != SOCK_DGRAM) && (type != SOCK_SEQPACKET)) {
		return -EPROTOTYPE;
	}

	if (protocol != PF_UNSPEC) {
		return -EPROTONOSUPPORT;
	}

	ns = usocket_alloc(type, nonblock);
	if (ns == NULL) {
		return -ENOMEM;
	}

	*s = ns;

	return EOK;
}


int usocket_socketpair(int domain, unsigned int type, int protocol, usocket_t *sv[2])
{
	usocket_t *s[2];
	uchannel_t *ch[2];
	int nonblock, framed;
	size_t size;

	nonblock = ((type & SOCK_NONBLOCK) != 0U) ? 1 : 0;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);

	if ((type != SOCK_STREAM) && (type != SOCK_DGRAM) && (type != SOCK_SEQPACKET)) {
		return -EPROTOTYPE;
	}

	if (protocol != PF_UNSPEC) {
		return -EPROTONOSUPPORT;
	}

	framed = (type != SOCK_STREAM) ? 1 : 0;
	size = USOCKET_DEF_BUFFER_SIZE;

	s[0] = usocket_alloc(type, nonblock);
	if (s[0] == NULL) {
		return -ENOMEM;
	}

	s[1] = usocket_alloc(type, nonblock);
	if (s[1] == NULL) {
		usocket_put(s[0]);
		return -ENOMEM;
	}

	ch[0] = uchannel_alloc(size, framed);
	if (ch[0] == NULL) {
		usocket_put(s[0]);
		usocket_put(s[1]);
		return -ENOMEM;
	}

	ch[1] = uchannel_alloc(size, framed);
	if (ch[1] == NULL) {
		uchannel_put(ch[0]);
		usocket_put(s[0]);
		usocket_put(s[1]);
		return -ENOMEM;
	}

	s[0]->rx = ch[0];
	s[0]->tx = uchannel_ref(ch[1]);
	s[1]->rx = ch[1];
	s[1]->tx = uchannel_ref(ch[0]);
	s[0]->state = (u8)usocketConnected;
	s[1]->state = (u8)usocketConnected;

	sv[0] = s[0];
	sv[1] = s[1];

	return EOK;
}


int usocket_bind(usocket_t *s, const struct sockaddr *address, socklen_t address_len)
{
	char *path, *name;
	const char *dir;
	oid_t odir, dev, node;
	int err, id;

	if ((address == NULL) || (address->sa_family != (sa_family_t)AF_UNIX)) {
		return -EINVAL;
	}

	(void)proc_lockSet(&s->lock);
	if (s->state != (u8)usocketUnconnected) {
		(void)proc_lockClear(&s->lock);
		return -EINVAL;
	}
	s->state = (u8)usocketBinding;
	(void)proc_lockClear(&s->lock);

	/*
	 * usocketBinding gives exclusive access to the naming of this socket,
	 * so the two message exchanges below run without any lock held.
	 */

	path = lib_strdup(address->sa_data);
	id = (path != NULL) ? usocket_nameAlloc(s) : -ENOMEM;

	if (id < 0) {
		(void)proc_lockSet(&s->lock);
		s->state = (u8)usocketUnconnected;
		(void)proc_lockClear(&s->lock);
		vm_kfree(path);
		return id;
	}

	lib_splitname(path, &name, &dir);

	/*
	 * The socket becomes reachable by its name below, so a datagram socket has
	 * to have somewhere to receive into before the socket file is created.
	 */
	err = (s->type == SOCK_DGRAM) ? usocket_rxCreate(s) : EOK;

	if (err == EOK) {
		if (proc_lookup(dir, NULL, &odir) < 0) {
			err = -ENOENT;
		}
		else {
			dev.port = USOCKET_PORT;
			dev.id = (id_t)id;
			err = proc_create(odir.port, 2 /* otDev */, S_IFSOCK, dev, odir, name, &node);
			if (err == -EEXIST) {
				err = -EADDRINUSE;
			}
		}
	}

	vm_kfree(path);

	(void)proc_lockSet(&s->lock);
	s->state = (u8)usocketUnconnected;
	(void)proc_lockClear(&s->lock);

	if (err != 0) {
		if (usocket_nameFree(s) != 0) {
			usocket_put(s);
		}
	}

	return err;
}


int usocket_listen(usocket_t *s, int backlog)
{
	unsigned int limit;
	int err;

	if ((s->type != SOCK_STREAM) && (s->type != SOCK_SEQPACKET)) {
		return -EOPNOTSUPP;
	}

	limit = (backlog <= 0) ? 1U : min((unsigned int)backlog, USOCKET_MAX_BACKLOG);

	(void)proc_lockSet(&s->lock);

	if (s->state == (u8)usocketUnconnected) {
		s->state = (u8)usocketListening;
		s->backlog = (u8)limit;
		err = EOK;
	}
	else if (s->state == (u8)usocketListening) {
		/* POSIX allows listen() to be called again to change the backlog */
		s->backlog = (u8)limit;
		err = EOK;
	}
	else {
		err = -EINVAL;
	}

	(void)proc_lockClear(&s->lock);

	return err;
}


/* Makes a connector that is (or was) queued on a listener give up. */
static void usocket_abort(usocket_t *s, int err)
{
	(void)proc_lockSet(&s->lock);

	if (s->state == (u8)usocketConnecting) {
		s->state = (u8)usocketAborted;
		s->err = err;
		(void)proc_threadBroadcast(&s->connq);
	}

	(void)proc_lockClear(&s->lock);
}


/*
 * Undoes the state change of a connect() that never made it onto the
 * listener's pending list, together with the receive channel it allocated for
 * the connection (a datagram socket keeps its own, which is not part of any
 * connection).
 */
static void usocket_connectRollback(usocket_t *s)
{
	uchannel_t *rx = NULL;

	(void)proc_lockSet(&s->lock);

	s->state = (u8)usocketUnconnected;
	if (s->type != SOCK_DGRAM) {
		rx = s->rx;
		s->rx = NULL;
	}

	(void)proc_lockClear(&s->lock);

	uchannel_put(rx);
}


/* TODO: add support for disconnecting and reconnecting a SOCK_DGRAM socket using AF_UNSPEC. */
int usocket_connect(usocket_t *s, const struct sockaddr *address, socklen_t address_len)
{
	usocket_t *ls;
	uchannel_t *ch, *rx, *old_tx;
	oid_t oid;
	size_t rcvbuf;
	int err, nonblock;

	if ((address == NULL) || (address->sa_family != (sa_family_t)AF_UNIX)) {
		return -EINVAL;
	}

	if ((s->type != SOCK_STREAM) && (s->type != SOCK_SEQPACKET) && (s->type != SOCK_DGRAM)) {
		return -EOPNOTSUPP;
	}

	if (proc_lookup(address->sa_data, NULL, &oid) < 0) {
		return -ECONNREFUSED;
	}

	if (oid.port != USOCKET_PORT) {
		return -ECONNREFUSED;
	}

	(void)proc_lockSet(&s->lock);

	switch (s->state) {
		case usocketUnconnected:
			err = EOK;
			break;
		case usocketConnecting:
			err = -EALREADY;
			break;
		case usocketConnected:
			err = -EISCONN;
			break;
		case usocketListening:
			err = -EOPNOTSUPP;
			break;
		case usocketBinding:
			/* a concurrent bind() owns the naming of this socket */
			err = -EINVAL;
			break;
		default:
			/*
			 * usocketAborted: POSIX leaves the state of a socket with a failed
			 * connect() unspecified, it has to be closed and created anew.
			 * usocketClosed cannot be seen here, as close() runs only once the
			 * last descriptor reference to the socket is gone.
			 */
			err = -EINVAL;
			break;
	}
	if (err == EOK) {
		s->state = (u8)usocketConnecting;
	}
	rcvbuf = s->rcvbuf;

	(void)proc_lockClear(&s->lock);

	if (err != EOK) {
		return err;
	}

	ls = usocket_get(oid.id);
	if (ls == NULL) {
		usocket_connectRollback(s);
		return -ECONNREFUSED;
	}

	if (ls->type != s->type) {
		usocket_put(ls);
		usocket_connectRollback(s);
		return -EPROTOTYPE;
	}

	if (s->type == SOCK_DGRAM) {
		/*
		 * No handshake: caching a reference to the peer's receive channel is
		 * the whole of a datagram connection.
		 */
		(void)proc_lockSet(&ls->lock);
		ch = uchannel_ref(ls->rx);
		(void)proc_lockClear(&ls->lock);
		usocket_put(ls);

		if (ch == NULL) {
			usocket_connectRollback(s);
			return -ECONNREFUSED;
		}

		(void)proc_lockSet(&s->lock);
		old_tx = s->tx;
		s->tx = ch;
		s->state = (u8)usocketConnected;
		(void)proc_lockClear(&s->lock);

		/* uchannel_put() can reach into the descriptor table, so hold no lock */
		uchannel_put(old_tx);

		return EOK;
	}

	/*
	 * The connector allocates and sizes the channel it will read from.
	 * The acceptor takes a reference to it as its own transmit channel.
	 * Every field of this socket is therefore still touched only under
	 * its own lock.
	 */
	rx = uchannel_alloc(rcvbuf, usocket_isFramed(s));
	if (rx == NULL) {
		usocket_put(ls);
		usocket_connectRollback(s);
		return -ENOMEM;
	}

	(void)proc_lockSet(&s->lock);
	uchannel_put(s->rx);
	s->rx = rx;
	(void)proc_lockClear(&s->lock);

	(void)proc_lockSet(&ls->lock);
	if ((ls->state != (u8)usocketListening) || (ls->pendingCnt >= ls->backlog)) {
		err = -ECONNREFUSED;
	}
	else {
		(void)usocket_ref(s);
		LIST_ADD(&ls->pending, s);
		ls->pendingCnt++;
		(void)proc_threadBroadcast(&ls->acceptq);
		err = EOK;
	}
	(void)proc_lockClear(&ls->lock);

	usocket_put(ls);

	if (err != EOK) {
		usocket_connectRollback(s);
		return err;
	}

	(void)proc_lockSet(&s->lock);

	nonblock = ((s->flags & USOCKET_NONBLOCK) != 0U) ? 1 : 0;

	while (s->state == (u8)usocketConnecting) {
		if (nonblock != 0) {
			(void)proc_lockClear(&s->lock);
			return -EINPROGRESS;
		}

		err = proc_lockWait(&s->connq, &s->lock, 0);
		if (err == -EINTR) {
			/*
			 * The lock has not been reacquired. Leave the socket queued on the
			 * listener, which will drop it when it gets there.
			 */
			usocket_abort(s, EINTR);
			return -EINTR;
		}
		if (err < 0) {
			(void)proc_lockClear(&s->lock);
			usocket_abort(s, -err);
			return err;
		}
	}

	if (s->state == (u8)usocketConnected) {
		err = EOK;
		ch = NULL;
	}
	else {
		/*
		 * The listener is gone. Tt has already taken us off its pending list,
		 * so the socket can be used for another attempt.
		 */
		err = (s->err != 0) ? -s->err : -ECONNREFUSED;
		s->err = 0;
		s->state = (u8)usocketUnconnected;
		ch = s->rx;
		s->rx = NULL;
	}

	(void)proc_lockClear(&s->lock);

	uchannel_put(ch);

	return err;
}


int usocket_accept4(usocket_t *ls, struct sockaddr *address, socklen_t *address_len, unsigned int flags, usocket_t **s)
{
	usocket_t *c, *ns;
	uchannel_t *c2s, *tx, *old_tx;
	size_t rcvbuf;
	int err, nonblock;

	if ((ls->type != SOCK_STREAM) && (ls->type != SOCK_SEQPACKET)) {
		return -EOPNOTSUPP;
	}

	nonblock = ((flags & SOCK_NONBLOCK) != 0U) ? 1 : 0;

	for (;;) {
		(void)proc_lockSet(&ls->lock);

		while ((ls->pending == NULL) && (ls->state == (u8)usocketListening)) {
			if ((ls->flags & USOCKET_NONBLOCK) != 0U) {
				(void)proc_lockClear(&ls->lock);
				return -EWOULDBLOCK;
			}

			err = proc_lockWait(&ls->acceptq, &ls->lock, 0);
			if (err == -EINTR) {
				/* the lock has not been reacquired */
				return -EINTR;
			}
			if (err < 0) {
				(void)proc_lockClear(&ls->lock);
				return err;
			}
		}

		if (ls->state != (u8)usocketListening) {
			(void)proc_lockClear(&ls->lock);
			return -EINVAL;
		}

		c = ls->pending;
		LIST_REMOVE(&ls->pending, c);
		ls->pendingCnt--;
		rcvbuf = ls->rcvbuf;

		(void)proc_lockClear(&ls->lock);

		/* the pending list's reference to the connector is ours now */

		ns = usocket_alloc(ls->type, nonblock);
		if (ns == NULL) {
			usocket_abort(c, ECONNREFUSED);
			usocket_put(c);
			return -ENOMEM;
		}

		c2s = uchannel_alloc(rcvbuf, usocket_isFramed(ls));
		if (c2s == NULL) {
			usocket_put(ns);
			usocket_abort(c, ECONNREFUSED);
			usocket_put(c);
			return -ENOMEM;
		}

		/*
		 * The one and only place where a socket touches another socket's
		 * endpoint: handing the channels over to the connector.
		 */
		(void)proc_lockSet(&c->lock);

		if ((c->state != (u8)usocketConnecting) || (c->rx == NULL)) {
			/*
			 * The connector gave up (interrupted, or its descriptor was
			 * closed after a non-blocking connect) - try the next one.
			 */
			(void)proc_lockClear(&c->lock);
			usocket_put(c);
			uchannel_put(c2s);
			usocket_put(ns);
			continue;
		}

		tx = uchannel_ref(c->rx);
		old_tx = c->tx;
		c->tx = uchannel_ref(c2s);
		c->state = (u8)usocketConnected;
		(void)proc_threadBroadcast(&c->connq);

		(void)proc_lockClear(&c->lock);

		/* uchannel_put() can reach into the descriptor table, so hold no lock */
		uchannel_put(old_tx);
		usocket_put(c);

		/* ns is not reachable yet */
		ns->rcvbuf = rcvbuf;
		ns->rx = c2s;
		ns->tx = tx;
		ns->state = (u8)usocketConnected;

		*s = ns;

		return EOK;
	}
}


int usocket_getpeername(usocket_t *s, struct sockaddr *address, socklen_t *address_len)
{
	return 0;
}


int usocket_getsockname(usocket_t *s, struct sockaddr *address, socklen_t *address_len)
{
	return 0;
}


int usocket_getsockopt(usocket_t *s, int level, int optname, void *optval, socklen_t *optlen)
{
	uchannel_t *rx;
	size_t size;
	int err = EOK;
	int value;

	if (level != SOL_SOCKET) {
		return -EINVAL;
	}

	if ((optval == NULL) || (optlen == NULL) || (*optlen < sizeof(int))) {
		return -EINVAL;
	}

	switch ((unsigned int)optname) {
		case SO_RCVBUF:
			(void)proc_lockSet(&s->lock);
			size = s->rcvbuf;
			rx = uchannel_ref(s->rx);
			(void)proc_lockClear(&s->lock);

			if (rx != NULL) {
				size = uchannel_size(rx);
				uchannel_put(rx);
			}

			value = (int)size;
			break;

		case SO_ERROR:
			/* POSIX: the pending error is returned in optval and cleared */
			(void)proc_lockSet(&s->lock);
			value = s->err;
			s->err = 0;
			(void)proc_lockClear(&s->lock);
			break;

		default:
			err = -ENOPROTOOPT;
			break;
	}

	if (err == EOK) {
		*((int *)optval) = value;
		*optlen = (socklen_t)sizeof(int);
	}

	return err;
}


static size_t usocket_bufferSize(size_t size)
{
	return uchannel_roundSize(min(max(size, USOCKET_MIN_BUFFER_SIZE), USOCKET_MAX_BUFFER_SIZE));
}


int usocket_setsockopt(usocket_t *s, int level, int optname, const void *optval, socklen_t optlen)
{
	uchannel_t *rx;
	size_t size;
	int err, val;

	if (level != SOL_SOCKET) {
		return -EINVAL;
	}

	switch ((unsigned int)optname) {
		case SO_RCVBUF:
			if ((optval == NULL) || (optlen != sizeof(int))) {
				err = -EINVAL;
				break;
			}

			val = *((const int *)optval);
			if (val < 0) {
				err = -EINVAL;
				break;
			}

			size = usocket_bufferSize((size_t)val);

			(void)proc_lockSet(&s->lock);
			rx = uchannel_ref(s->rx);
			(void)proc_lockClear(&s->lock);

			err = EOK;
			if (rx != NULL) {
				err = uchannel_resize(rx, size);
				uchannel_put(rx);
			}

			if (err == EOK) {
				(void)proc_lockSet(&s->lock);
				s->rcvbuf = size;
				(void)proc_lockClear(&s->lock);
			}
			break;

		default:
			err = -ENOPROTOOPT;
			break;
	}

	return err;
}


static ssize_t usocket_recv(usocket_t *s, void *buf, size_t len, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len, void *control, socklen_t *controllen)
{
	uchannel_t *rx;
	fdpack_t *packs = NULL;
	ssize_t ret;
	unsigned int op;
	int wantsControl, shutRd, err;

	wantsControl = ((control != NULL) && (controllen != NULL) && (*controllen > 0U)) ? 1 : 0;

	(void)proc_lockSet(&s->lock);
	op = usocket_opFlags(s, flags);
	rx = uchannel_ref(s->rx);
	shutRd = ((s->flags & USOCKET_SHUT_RD) != 0U) ? 1 : 0;
	(void)proc_lockClear(&s->lock);

	if ((src_addr != NULL) && (src_len != NULL)) {
		/* the peer of a UNIX socket has no address to report here */
		*src_len = 0;
	}

	err = EOK;
	if ((rx == NULL) && (s->type == SOCK_DGRAM) && (shutRd == 0)) {
		err = usocket_rxCreate(s);
		if (err == EOK) {
			(void)proc_lockSet(&s->lock);
			rx = uchannel_ref(s->rx);
			(void)proc_lockClear(&s->lock);
		}
	}

	if (rx == NULL) {
		if (controllen != NULL) {
			*controllen = 0;
		}

		if (err != EOK) {
			return (ssize_t)err;
		}

		/* a socket shut down for reading is EOS, not unconnected */
		return (shutRd != 0) ? 0 : -ENOTCONN;
	}

	ret = uchannel_read(rx, buf, len, op, (wantsControl != 0) ? &packs : NULL);

	if (packs != NULL) {
		/*
		 * No lock is held here - fdpass_unpack() reaches into the file
		 * descriptor table of this process.
		 */
		(void)fdpass_unpack(&packs, control, controllen);
		if (packs != NULL) {
			uchannel_returnPacks(rx, &packs);
		}
	}
	else {
		if (controllen != NULL) {
			*controllen = 0;
		}
	}

	uchannel_put(rx);

	return ret;
}


static ssize_t usocket_send(usocket_t *s, const void *buf, size_t len, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len, fdpack_t *fdpack)
{
	usocket_t *d;
	uchannel_t *tx, *old_tx = NULL;
	oid_t oid;
	ssize_t ret;
	unsigned int op;
	int dgram, err;

	dgram = (s->type == SOCK_DGRAM) ? 1 : 0;

	if ((dgram != 0) && (dest_addr != NULL) && (dest_len != 0U)) {
		if (dest_addr->sa_family != (sa_family_t)AF_UNIX) {
			return -EINVAL;
		}

		if (proc_lookup(dest_addr->sa_data, NULL, &oid) < 0) {
			return -ECONNREFUSED;
		}

		if (oid.port != USOCKET_PORT) {
			return -ECONNREFUSED;
		}

		d = usocket_get(oid.id);
		if (d == NULL) {
			return -ECONNREFUSED;
		}

		if (d->type != s->type) {
			usocket_put(d);
			return -EPROTOTYPE;
		}

		(void)proc_lockSet(&d->lock);
		tx = uchannel_ref(d->rx);
		(void)proc_lockClear(&d->lock);

		usocket_put(d);

		(void)proc_lockSet(&s->lock);
		op = usocket_opFlags(s, flags);
		err = ((s->flags & USOCKET_SHUT_WR) != 0U) ? -EPIPE : EOK;
		(void)proc_lockClear(&s->lock);

		if (err != EOK) {
			uchannel_put(tx);
			return (ssize_t)err;
		}

		if (tx == NULL) {
			return -ECONNREFUSED;
		}

		ret = uchannel_write(tx, buf, len, op, fdpack);

		uchannel_put(tx);

		return (ret == -EPIPE) ? -ECONNREFUSED : ret;
	}

	if ((dgram == 0) && ((dest_addr != NULL) || (dest_len != 0U))) {
		return -EISCONN;
	}

	(void)proc_lockSet(&s->lock);
	op = usocket_opFlags(s, flags);
	if ((s->flags & USOCKET_SHUT_WR) != 0U) {
		err = -EPIPE;
	}
	else {
		tx = uchannel_ref(s->tx);
		err = (tx == NULL) ? -ENOTCONN : EOK;
	}
	(void)proc_lockClear(&s->lock);

	if (err != EOK) {
		return (ssize_t)err;
	}

	ret = uchannel_write(tx, buf, len, op, fdpack);

	if ((dgram != 0) && (ret == -EPIPE)) {
		/*
		 * A datagram peer that is gone is reported once as ECONNREFUSED, and
		 * the socket is disconnected, so that a next send() gets ENOTCONN
		 * and the socket can be connected again.
		 */
		(void)proc_lockSet(&s->lock);
		if (s->tx == tx) {
			old_tx = s->tx;
			s->tx = NULL;
			s->state = (u8)usocketUnconnected;
		}
		(void)proc_lockClear(&s->lock);

		/* uchannel_put() can reach into the descriptor table, so hold no lock */
		uchannel_put(old_tx);

		ret = -ECONNREFUSED;
	}

	uchannel_put(tx);

	return ret;
}


ssize_t usocket_recvfrom(usocket_t *s, void *msg, size_t len, unsigned int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	return usocket_recv(s, msg, len, flags, src_addr, src_len, NULL, NULL);
}


ssize_t usocket_sendto(usocket_t *s, const void *msg, size_t len, unsigned int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	return usocket_send(s, msg, len, flags, dest_addr, dest_len, NULL);
}


ssize_t usocket_recvmsg(usocket_t *s, struct msghdr *msg, unsigned int flags)
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

	err = usocket_recv(s, buf, len, flags, msg->msg_name, &msg->msg_namelen, msg->msg_control, &msg->msg_controllen);

	if (err >= 0) {
		/* output flags are not supported */
		msg->msg_flags = 0;
	}

	return err;
}


ssize_t usocket_sendmsg(usocket_t *s, const struct msghdr *msg, unsigned int flags)
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

	err = usocket_send(s, buf, len, flags, msg->msg_name, msg->msg_namelen, fdpack);

	/* file descriptors are passed only when some bytes have been sent */
	if (fdpack != NULL && err <= 0) {
		(void)fdpass_discard(&fdpack);
	}

	return err;
}


int usocket_shutdown(usocket_t *s, int how)
{
	uchannel_t *rx = NULL, *tx = NULL;

	if ((how != SHUT_RD) && (how != SHUT_WR) && (how != SHUT_RDWR)) {
		return -EINVAL;
	}

	(void)proc_lockSet(&s->lock);

	if (s->state != (u8)usocketConnected) {
		(void)proc_lockClear(&s->lock);
		return -ENOTCONN;
	}

	if (how != SHUT_RD) {
		s->flags |= USOCKET_SHUT_WR;
		/*
		 * A datagram socket transmits into the destination's receive channel,
		 * which every other sender shares, so it must not be marked - the
		 * endpoint flag alone stops this socket from sending.
		 */
		if (s->type != SOCK_DGRAM) {
			tx = uchannel_ref(s->tx);
		}
	}

	if (how != SHUT_WR) {
		s->flags |= USOCKET_SHUT_RD;
		rx = uchannel_ref(s->rx);
	}

	(void)proc_lockClear(&s->lock);

	/*
	 * The flags are set on the channels, so the peer sees end-of-stream on the
	 * direction we stopped writing and EPIPE on the one we stopped reading,
	 * without either side touching the other's socket.
	 */

	if (tx != NULL) {
		uchannel_shutWr(tx);
		uchannel_put(tx);
	}

	if (rx != NULL) {
		uchannel_shutRd(rx);
		uchannel_put(rx);
	}

	return EOK;
}


int usocket_setfl(usocket_t *s, unsigned int flags)
{
	(void)proc_lockSet(&s->lock);
	if ((flags & O_NONBLOCK) != 0U) {
		s->flags |= USOCKET_NONBLOCK;
	}
	else {
		s->flags &= ~USOCKET_NONBLOCK;
	}
	(void)proc_lockClear(&s->lock);

	return 0;
}


int usocket_getfl(usocket_t *s)
{
	/* the access mode of a socket is fixed - shutdown() does not change it */
	unsigned int flags = O_RDWR;

	(void)proc_lockSet(&s->lock);
	if ((s->flags & USOCKET_NONBLOCK) != 0U) {
		flags |= O_NONBLOCK;
	}
	(void)proc_lockClear(&s->lock);

	return (int)flags;
}


/*
 * Called after the socket file has been removed from the filesystem: the name
 * must stop resolving to this socket, while the socket itself stays usable for
 * its already connected peers.
 */
int usocket_unlink(id_t id)
{
	usocket_t *s;

	s = usocket_get(id);
	if (s == NULL) {
		return EOK;
	}

	if (usocket_nameFree(s) != 0) {
		usocket_put(s);
	}

	usocket_put(s);

	return EOK;
}


int usocket_close(usocket_t *s)
{
	usocket_t *pending, *next;
	uchannel_t *rx, *tx;
	int shutTx;

	(void)proc_lockSet(&s->lock);

	s->state = (u8)usocketClosed;
	rx = s->rx;
	tx = s->tx;
	s->rx = NULL;
	s->tx = NULL;
	/* as in usocket_shutdown(), a shared datagram channel is not ours to mark */
	shutTx = (s->type != SOCK_DGRAM) ? 1 : 0;
	pending = s->pending;
	s->pending = NULL;
	s->pendingCnt = 0;

	/* wake anything blocked on this socket itself */
	(void)proc_threadBroadcast(&s->acceptq);
	(void)proc_threadBroadcast(&s->connq);

	if (pending != NULL) {
		/*
		 * Break the ring into a NULL terminated chain while the list is still
		 * private, so that the walk below never has to look at an entry that
		 * has already been woken (and may have queued itself elsewhere).
		 */
		pending->prev->next = NULL;
	}

	(void)proc_lockClear(&s->lock);

	if (tx != NULL) {
		if (shutTx != 0) {
			uchannel_shutWr(tx);
		}
		uchannel_put(tx);
	}

	if (rx != NULL) {
		uchannel_shutRd(rx);
		/*
		 * The reader is gone, so queued descriptors can never be delivered.
		 *
		 * TODO: a descriptor of this socket queued in the very channel this
		 * socket reads from is not covered here. The pack keeps the socket's
		 * open file referenced, that reference keeps the socket alive, and the
		 * socket keeps the channel alive - so the open file never reaches zero
		 * references and this function is never reached for it. The endpoint,
		 * both its channels and their ring buffers then stay allocated until
		 * reboot.
		 */
		uchannel_discardPacks(rx);
		uchannel_put(rx);
	}

	while (pending != NULL) {
		next = pending->next;
		usocket_abort(pending, ECONNREFUSED);
		usocket_put(pending);
		pending = next;
	}

	if (usocket_nameFree(s) != 0) {
		usocket_put(s);
	}

	/* drop the descriptor's reference */
	usocket_put(s);

	return EOK;
}


int usocket_poll(usocket_t *s, unsigned short events)
{
	uchannel_t *rx, *tx;
	unsigned int revents = 0;
	unsigned int ev;
	int state, pending;

	(void)proc_lockSet(&s->lock);

	state = (int)s->state;
	pending = (s->pending != NULL) ? 1 : 0;
	rx = uchannel_ref(s->rx);
	tx = uchannel_ref(s->tx);

	(void)proc_lockClear(&s->lock);

	if (state == usocketListening) {
		if (pending != 0) {
			revents |= (unsigned int)events & (POLLIN | POLLRDNORM | POLLRDBAND);
		}
	}
	else if (state == usocketAborted) {
		/* a failed connect() - POLLERR and POLLHUP are reported unconditionally */
		revents |= POLLERR | POLLHUP;
	}
	else {
		if (rx != NULL) {
			ev = uchannel_pollRd(rx);
			if ((ev & UCHANNEL_EV_IN) != 0U) {
				revents |= (unsigned int)events & (POLLIN | POLLRDNORM | POLLRDBAND);
			}
			if ((ev & UCHANNEL_EV_HUP) != 0U) {
				revents |= POLLHUP;
			}
		}

		if (tx != NULL) {
			ev = uchannel_pollWr(tx);
			if ((ev & UCHANNEL_EV_OUT) != 0U) {
				revents |= (unsigned int)events & (POLLOUT | POLLWRNORM | POLLWRBAND);
			}
		}
	}

	uchannel_put(rx);
	uchannel_put(tx);

	return (int)revents;
}


void usocket_init(void)
{
	lib_idtreeInit(&usocket_common.tree);
	usocket_common.nextId = 1;
	(void)proc_lockInit(&usocket_common.lock, &proc_lockAttrDefault, "unix.common");
}
