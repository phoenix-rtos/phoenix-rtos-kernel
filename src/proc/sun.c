/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * UNIX domain sockets
 *
 * Copyright 2018-2020 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../include/errno.h"
#include "../../include/fcntl.h"
#include "../../include/socket.h"
#include "../proc/proc.h"
#include "../lib/lib.h"
#include "file.h"

#define DEBUG_LOG(fmt, ...) lib_printf("%s:%d  %s(): " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define SUN_BOUND (1 << 0)
#define SUN_LISTENING (1 << 1)
#define SUN_ACCEPTING (1 << 2)
#define SUN_CONNECTING (1 << 3)

#define SFL_CONNECTION_MODE (1 << 0)
#define SFL_STREAM (1 << 1)

enum {
	ss_new, ss_bound, ss_accepting,
	ss_connected, ss_closed
};

typedef struct _sun_t {
	rbnode_t linkage;

	port_t *address_port;
	id_t address_id;

	lock_t lock;
	fifo_t fifo;
	wait_note_t *wait;
	int refs;

	char state;
	char type;
	char flags;

	struct _sun_t *connection, *next, *prev;
} sun_t;


struct {
	rbtree_t bound;
	lock_t lock;
} sun_common;


static int sun_cmp(rbnode_t *n1, rbnode_t *n2)
{
	sun_t *s1 = lib_treeof(sun_t, linkage, n1);
	sun_t *s2 = lib_treeof(sun_t, linkage, n2);
	int rv;

	if ((rv = (s1->address_port > s2->address_port) - (s1->address_port < s2->address_port)))
		return rv;

	return (s1->address_id > s2->address_id) - (s1->address_id < s2->address_id);
}


static sun_t *sun_find(port_t *port, id_t id)
{
	sun_t t, *r;
	t.address_port = port;
	t.address_id = id;
	if ((r = lib_treeof(sun_t, linkage, lib_rbFind(&sun_common.bound, &t.linkage))) != NULL)
		r->refs++;
	return r;
}


static int sun_destroy(sun_t *socket)
{
	proc_lockDone(&socket->lock);
	if (socket->address_port != NULL) {
		proc_objectClose(socket->address_port, socket->address_id);
	}
	vm_kfree(socket->fifo.data);
	return EOK;
}


static void sun_put(sun_t *socket)
{
	if (socket != NULL && (!--socket->refs)) {
		sun_destroy(socket);
		vm_kfree(socket);
	}
}


/* TODO */
int sun_close(sun_t *socket)
{
	proc_lockSet(&sun_common.lock);
	if (socket->connection != NULL) {
		poll_signal(&socket->connection->wait, POLLHUP);
		sun_put(socket->connection);
	}
	sun_put(socket);
	proc_lockClear(&sun_common.lock);
	return EOK;
}


static int sun_init(sun_t *socket)
{
	hal_memset(socket, 0, sizeof(*socket));
	if ((socket->fifo.data = vm_kmalloc(SIZE_PAGE)) == NULL)
		return -ENOMEM;
	fifo_init(&socket->fifo, SIZE_PAGE);
	proc_lockInit(&socket->lock);
	socket->refs = 1;
	return EOK;
}


int sun_socket(process_t *process, int type, int protocol, int flags)
{
	sun_t *socket;
	iodes_t *file;
	int handle;

	if (type != SOCK_STREAM && type != SOCK_DGRAM) {
		DEBUG_LOG("invalid socket type: %d", type);
		return -EINVAL;
	}

	if (flags & ~(O_NONBLOCK | O_CLOEXEC))
		return -EINVAL;

	if ((socket = vm_kmalloc(sizeof(*socket))) == NULL)
		return -ENOMEM;

	if ((file = file_alloc()) == NULL) {
		vm_kfree(socket);
		return -ENOMEM;
	}

	if (sun_init(socket) < 0) {
		file_put(file);
		vm_kfree(socket);
		return -ENOMEM;
	}

	if (type == SOCK_STREAM || type == SOCK_SEQPACKET)
		socket->flags |= SFL_CONNECTION_MODE;

	if (type == SOCK_STREAM)
		socket->flags |= SFL_STREAM;

	file->sun = socket;
	file->type = ftLocalSocket;

	if ((handle = fd_new(process, 0, flags, file)) < 0)
		file_put(file);

	return handle;
}


int sun_bind(process_t *process, struct _sun_t *socket, const struct sockaddr *address, socklen_t address_len)
{
	int error, len;
	port_t *port;
	id_t id;

	if (socket->state & SUN_BOUND)
		return -EINVAL;

	if ((error = proc_sunCreate(&port, &id, AT_FDCWD, address->sa_data, 0755)) < 0)
		return error;

	proc_lockSet(&sun_common.lock);
	socket->address_port = port;
	socket->address_id = id;
	if ((error = lib_rbInsert(&sun_common.bound, &socket->linkage)) < 0) {
		error = -EADDRINUSE;
		proc_objectClose(port, id);
		socket->address_port = NULL;
		socket->address_id = 0;
	}
	proc_lockClear(&sun_common.lock);

	return error;
}


// TODO: handle accepting and connecting sockets
int sun_poll(sun_t *socket, poll_head_t *poll, wait_note_t *note)
{
	int events = 0;
	proc_lockSet(&sun_common.lock);
	poll_add(poll, &socket->wait, note);

	if (socket->fifo.data != NULL) {
		if (!fifo_is_empty(&socket->fifo))
			events |= POLLIN;

		if (!fifo_is_full(&socket->fifo))
			events |= POLLOUT;
	}

	proc_lockClear(&sun_common.lock);
	return events;
}


int sun_listen(struct _sun_t *socket, int backlog)
{
	int error;
	proc_lockSet(&sun_common.lock);
	if (socket->state & SUN_LISTENING) {
		error = -EADDRINUSE;
	}
	else if (!(socket->flags & SFL_CONNECTION_MODE)) {
		error =  -EOPNOTSUPP;
	}
	else {
		error = EOK;
		socket->state |= SUN_LISTENING;
		vm_kfree(socket->fifo.data);
		socket->fifo.data = NULL;
	}
	proc_lockClear(&sun_common.lock);
	return error;
}


int _sun_accept(process_t *process, struct _sun_t *socket, struct sockaddr *address, socklen_t *address_len)
{
	sun_t *peer, *new;
	iodes_t *file;
	int handle;

	if (!(socket->flags & SFL_CONNECTION_MODE))
		return -EOPNOTSUPP;

	if (!(socket->state & SUN_LISTENING))
		return -EINVAL;

	if ((peer = socket->connection) == NULL)
		return -EAGAIN;

	if ((new = vm_kmalloc(sizeof(*new))) == NULL)
		return -ENOMEM;

	if ((file = file_alloc()) == NULL) {
		vm_kfree(new);
		return -ENOMEM;
	}

	if (sun_init(new) < 0) {
		file_put(file);
		vm_kfree(new);
		return -ENOMEM;
	}

	file->sun = new;
	file->type = ftLocalSocket;

	if ((handle = fd_new(process, 0, 0, file)) < 0) {
		file_put(file);
	}
	else {
		LIST_REMOVE(&socket->connection, peer);
		new->connection = peer;
		peer->connection = new;
		new->refs++;
		poll_signal(&peer->wait, POLLOUT);
	}

	return handle;
}


int sun_accept(process_t *process, struct _sun_t *socket, struct sockaddr *address, socklen_t *address_len)
{
	int retval;
	proc_lockSet(&sun_common.lock);
	retval = _sun_accept(process, socket, address, address_len);
	proc_lockClear(&sun_common.lock);
	return retval;
}


static int sun_lookup(process_t *process, port_t **port, id_t *id, const char *path)
{
	iodes_t *dir;
	const char *suname;
	mode_t mode;
	int error;

	if ((error = file_resolve(&dir, process, AT_FDCWD, path, O_PARENT | O_DIRECTORY)) < 0)
		return error;

	suname = file_basename(path);

	if ((error = proc_objectLookup(dir->fs.port, dir->fs.id, suname, hal_strlen(suname), 0, id, &mode, NULL)) == EOK) {
		if (S_ISSOCK(mode)) {
			*port = dir->fs.port;
		}
		else {
			error = -ECONNREFUSED;
			proc_objectClose(dir->fs.port, *id);
		}
	}

	file_put(dir);
	return error;
}


int sun_connect(process_t *process, struct _sun_t *socket, const struct sockaddr *address, socklen_t address_len)
{
	sun_t *peer;
	int error;
	port_t *port;
	id_t id;

	if (socket->state & SUN_LISTENING)
		return -EADDRINUSE;

	if (socket->connection != NULL)
		return -EISCONN;

	if ((error = sun_lookup(process, &port, &id, address->sa_data)) < 0)
		return error;

	proc_lockSet(&sun_common.lock);
	if ((peer = sun_find(port, id)) == NULL) {
		error = -ECONNREFUSED;
	}
	else if ((peer->flags & SFL_CONNECTION_MODE) && !(peer->state & SUN_LISTENING)) {
		error = -ECONNREFUSED;
	}
	else if (peer->flags & SFL_CONNECTION_MODE) {
		socket->refs++;
		LIST_ADD(&peer->connection, socket);
		socket->state |= SUN_CONNECTING;
		poll_signal(&peer->wait, POLLIN);
		error = EOK;
	}
	else /* DGRAM socket */ {
		peer->refs++;
		socket->connection = peer;
		error = EOK;
	}

	sun_put(peer);
	proc_lockClear(&sun_common.lock);

	proc_objectClose(port, id);
	return error;
}


typedef struct {
	ssize_t size;
} sun_header_t;


ssize_t _sun_sendmsg(sun_t *socket, const struct msghdr *msg, int flags)
{
	sun_t *peer;
	ssize_t bytes = 0, err;
	sun_header_t header;
	int i;

	if (socket->state & SUN_LISTENING)
		return -ENOTCONN;

	if ((peer = socket->connection) == NULL) {
		if (socket->flags & SFL_CONNECTION_MODE)
			return -ENOTCONN;

		DEBUG_LOG("TODO: find by address");
		return -ECONNREFUSED;
	}

	if (fifo_is_full(&peer->fifo)) {
		DEBUG_LOG("peer full");
		return -EAGAIN;
	}

	if (socket->flags & SFL_STREAM) {
		for (i = 0; i < msg->msg_iovlen; ++i) {
			err = fifo_write(&peer->fifo, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);

			if (err >= 0)
				bytes += err;
			else
				break;
		}
	}
	else {
		for (i = 0; i < msg->msg_iovlen; ++i)
			bytes += msg->msg_iov[i].iov_len;

		header.size = bytes;
		if (fifo_freespace(&peer->fifo) < sizeof(header) + bytes) {
			DEBUG_LOG("low fs %d (need %d)", fifo_freespace(&peer->fifo), sizeof(header) + bytes);
			bytes = -EAGAIN;
		}
		else {
			fifo_write(&peer->fifo, &header, sizeof(header));

			for (i = 0; i < msg->msg_iovlen; ++i)
				fifo_write(&peer->fifo, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		}
	}

	if (bytes > 0)
		poll_signal(&peer->wait, POLLIN);

	return bytes;
}


ssize_t sun_sendmsg(sun_t *socket, const struct msghdr *msg, int flags)
{
	ssize_t retval;
	proc_lockSet(&sun_common.lock);
	retval = _sun_sendmsg(socket, msg, flags);
	proc_lockClear(&sun_common.lock);
	return retval;
}


ssize_t sun_recvstream(fifo_t *fifo, struct iovec *iovec, int iovlen)
{
	int err, i;
	ssize_t bytes = 0;

	for (i = 0; i < iovlen; ++i) {
		err = fifo_read(fifo, iovec[i].iov_base, iovec[i].iov_len);

		if (err > 0)
			bytes += err;

		if (err < iovec[i].iov_len)
			break;
	}

	return bytes;
}


static ssize_t sun_recvdgram(fifo_t *fifo, struct iovec *iovec, int iovlen)
{
	int got, i;
	ssize_t remaining, bytes = 0;
	sun_header_t header;

	fifo_read(fifo, &header, sizeof(header));
	remaining = header.size;

	for (i = 0; i < iovlen && remaining; ++i) {
		got = fifo_read(fifo, iovec[i].iov_base, min(remaining, iovec[i].iov_len));

		if (got > 0) {
			bytes += got;
			remaining -= got;
		}

		if (got < iovec[i].iov_len)
			break;
	}

	if (remaining)
		return -bytes;

	return bytes;
}


ssize_t _sun_recvmsg(sun_t *socket, struct msghdr *msg, int flags)
{
	ssize_t bytes = 0;

	msg->msg_flags = 0;

	if (socket->state & SUN_LISTENING)
		return -ENOTCONN;

	if ((socket->flags & SFL_CONNECTION_MODE) && socket->connection == NULL)
		return -ENOTCONN;

	if (fifo_is_empty(&socket->fifo))
		return -EAGAIN;

	if (socket->flags & SFL_STREAM) {
		bytes = sun_recvstream(&socket->fifo, msg->msg_iov, msg->msg_iovlen);
	}
	else {
		bytes = sun_recvdgram(&socket->fifo, msg->msg_iov, msg->msg_iovlen);

		if (bytes < 0) {
			msg->msg_flags |= MSG_TRUNC;
			bytes = -bytes;
		}
	}

	if (bytes > 0 && socket->connection != NULL)
		poll_signal(&socket->connection->wait, POLLOUT);

	return bytes;
}


ssize_t sun_recvmsg(sun_t *socket, struct msghdr *msg, int flags)
{
	ssize_t retval;
	proc_lockSet(&sun_common.lock);
	retval = _sun_recvmsg(socket, msg, flags);
	proc_lockClear(&sun_common.lock);
	return retval;
}


ssize_t sun_read(sun_t *socket, void *data, size_t size)
{
	struct msghdr msg;
	struct iovec iov;
	hal_memset(&msg, 0, sizeof(msg));
	iov.iov_base = data;
	iov.iov_len = size;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	return sun_recvmsg(socket, &msg, 0);
}


ssize_t sun_write(sun_t *socket, void *data, size_t size)
{
	struct msghdr msg;
	struct iovec iov;
	hal_memset(&msg, 0, sizeof(msg));
	iov.iov_base = data;
	iov.iov_len = size;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	return sun_sendmsg(socket, &msg, 0);
}


void _sun_init(void)
{
	proc_lockInit(&sun_common.lock);
	lib_rbInit(&sun_common.bound, sun_cmp, NULL);
}
