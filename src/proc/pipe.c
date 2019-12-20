/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Pipes
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../../include/fcntl.h"
#include "../lib/lib.h"
#include "proc.h"
#include "file.h"

#define IS_POW_2(x) ((x) && !((x) & ((x) - 1)))

typedef struct _pipe_t {
	lock_t lock;
	fifo_t fifo;
	wait_note_t *wait;
	unsigned nreaders;
	unsigned nwriters;
} pipe_t;


typedef struct _named_pipe_t {
	pipe_t pipe;
	oid_t oid;
	rbnode_t linkage;
} named_pipe_t;


struct {
	rbtree_t named;
	lock_t lock;
} pipe_common;


static void pipe_lock(pipe_t *pipe)
{
	proc_lockSet(&pipe->lock);
}


static void pipe_unlock(pipe_t *pipe)
{
	proc_lockClear(&pipe->lock);
}


static int pipe_destroy(pipe_t *pipe)
{
	proc_lockDone(&pipe->lock);
	vm_kfree(pipe->fifo.data);
	return EOK;
}


int pipe_poll(pipe_t *pipe, poll_head_t *poll, wait_note_t *note)
{
	int events = 0;
	pipe_lock(pipe);
	poll_add(poll, &pipe->wait, note);

	if (!fifo_is_empty(&pipe->fifo))
		events |= POLLIN;

	if (!fifo_is_full(&pipe->fifo))
		events |= POLLOUT;

	if (!pipe->nreaders || !pipe->nwriters) {
		events |= POLLHUP;
		lib_printf("pipe hup\n");
	}

	pipe_unlock(pipe);
	return events;
}


ssize_t pipe_read(pipe_t *pipe, void *data, size_t size)
{
	ssize_t retval;

	pipe_lock(pipe);
	retval = fifo_read(&pipe->fifo, data, size);
	pipe_unlock(pipe);

	if (retval > 0)
		poll_signal(&pipe->wait, POLLOUT);
	else if (!retval && pipe->nwriters)
		retval = -EAGAIN;

	return retval;
}


ssize_t pipe_write(pipe_t *pipe, const void *data, size_t size)
{
	ssize_t retval = 0;
	int atomic;

	if (!pipe->nreaders)
		return -EPIPE;

	if (!size)
		return 0;

	pipe_lock(pipe);
	atomic = size <= fifo_size(&pipe->fifo);

	if (fifo_freespace(&pipe->fifo) > atomic * (size - 1))
		retval = fifo_write(&pipe->fifo, data, size);
	else
		retval = -EAGAIN;
	pipe_unlock(pipe);

	if (retval > 0)
		poll_signal(&pipe->wait, POLLIN);

	return retval;
}


int pipe_close(pipe_t *pipe, int read, int write)
{
	/* FIXME: races */
	if (read) {
		lib_atomicDecrement(&pipe->nreaders);
	}

	if (write) {
		lib_atomicDecrement(&pipe->nwriters);
	}

	if (!pipe->nreaders && !pipe->nwriters) {
		pipe_destroy(pipe);
		vm_kfree(pipe);
	}
	else {
		poll_signal(&pipe->wait, POLLHUP);
	}
	return EOK;
}


int pipe_ioctl(pipe_t *pipe, unsigned cmd, const void *in_buf, size_t in_size, void *out_buf, size_t out_size)
{
	lib_printf("pipe_ioctl\n");
	return -ENOSYS;
}


int pipe_init(pipe_t *pipe, size_t size)
{
	if (!IS_POW_2(size))
		return -EINVAL;

	if ((pipe->fifo.data = vm_kmalloc(size)) == NULL)
		return -ENOMEM;

	pipe->nreaders = pipe->nwriters = 0;
	pipe->wait = NULL;
	proc_lockInit(&pipe->lock);
	fifo_init(&pipe->fifo, size);
	return EOK;
}


int pipe_create(process_t *process, size_t size, int fds[2], int flags)
{
	int readfd = -1, writefd = -1, err = EOK;
	pipe_t *pipe = NULL;
	file_t *read_end, *write_end;
	int fdflags = 0;

	if (flags & ~(O_NONBLOCK | O_CLOEXEC))
		return -EINVAL;

	if (flags & O_CLOEXEC)
		fdflags = FD_CLOEXEC;

	if (!IS_POW_2(size))
		return -EINVAL;

	if ((pipe = vm_kmalloc(sizeof(pipe_t))) == NULL)
		return -ENOMEM;

	if ((read_end = file_alloc()) == NULL) {
		vm_kfree(pipe);
		return -ENOMEM;
	}

	if ((write_end = file_alloc()) == NULL) {
		file_put(read_end);
		vm_kfree(pipe);
		return -ENOMEM;
	}

	if ((err = pipe_init(pipe, size)) < 0) {
		file_put(read_end);
		file_put(write_end);
		vm_kfree(pipe);
		return err;
	}

	pipe->nreaders = 1;
	pipe->nwriters = 1;

	read_end->status = (flags & ~O_CLOEXEC) | O_RDONLY;
	write_end->status = (flags & ~O_CLOEXEC) | O_WRONLY;

	write_end->pipe = read_end->pipe = pipe;
	write_end->type = read_end->type = ftPipe;

	if ((readfd = fd_new(process, 0, fdflags, read_end)) < 0) {
		file_put(read_end);
		file_put(write_end);
		pipe_destroy(pipe);
		vm_kfree(pipe);
		return err;
	}

	if ((writefd = fd_new(process, 0, fdflags, write_end)) < 0) {
		file_put(write_end);
		fd_close(process, readfd);
		pipe_destroy(pipe);
		vm_kfree(pipe);
		return err;
	}

	fds[0] = readfd;
	fds[1] = writefd;
	return EOK;
}


int proc_pipeCreate(int fds[2], int flags)
{
	process_t *process = proc_current()->process;
	int retval;

	retval = pipe_create(process, SIZE_PAGE, fds, flags);
	return retval;
}


#if 0
int npipe_cmp(rbnode_t *n1, rbnode_t *n2)
{
	int cmp;

	named_pipe_t *p1 = lib_treeof(named_pipe_t, linkage, n1);
	named_pipe_t *p2 = lib_treeof(named_pipe_t, linkage, n2);

	if ((cmp = (p1->oid.port > p2->oid.port) - (p1->oid.port < p2->oid.port)))
		return cmp;

	return (p1->oid.id > p2->oid.id) - (p1->oid.id < p2->oid.id);
}


named_pipe_t *npipe_find(const oid_t *oid)
{
	named_pipe_t t;
	t.oid.port = oid->port;
	t.oid.id = oid->id;

	return lib_treeof(named_pipe_t, linkage, lib_rbFind(&pipe_common.named, &t.linkage));
}


named_pipe_t *npipe_create(const oid_t *oid)
{
	named_pipe_t *p;

	if ((p = vm_kmalloc(sizeof(*p))) != NULL) {
		p->oid.port = oid->port;
		p->oid.id = oid->id;

		if (pipe_init(&p->pipe, SIZE_PAGE) != EOK) {
			vm_kfree(p);
			return NULL;
		}

		if (lib_rbInsert(&pipe_common.named, &p->linkage) != EOK) {
			pipe_destroy(&p->pipe);
			vm_kfree(p);
			return NULL;
		}
	}
	return p;
}


void npipe_destroy(named_pipe_t *np)
{
	lib_rbRemove(&pipe_common.named, &np->linkage);
	pipe_destroy(&np->pipe);
	vm_kfree(np);
}


int npipe_release(file_t *file)
{
	named_pipe_t *npipe = file->pipe;
	pipe_t *pipe = &npipe->pipe;

	proc_lockSet(&pipe_common.lock);
	if (file->status & O_RDONLY) {
		lib_atomicDecrement(&pipe->nreaders);
	}
	if (file->status & O_WRONLY) {
		lib_atomicDecrement(&pipe->nwriters);
	}
	if (!pipe->nreaders && !pipe->nwriters) {
		npipe_destroy(file->pipe);
	}
	else {
		pipe_wakeup(pipe, POLLHUP);
	}
	proc_lockClear(&pipe_common.lock);

	return EOK;
}


int pipe_open(pipe_t *pipe, int oflag)
{
	int err = EOK;

	pipe_lock(pipe);
	if (oflag & O_RDONLY) {
		lib_atomicIncrement(&pipe->nreaders);
	}
	if (oflag & O_WRONLY) {
		lib_atomicIncrement(&pipe->nwriters);
	}
	pipe_wakeup(pipe, POLLIN | POLLOUT);
	if (oflag & O_NONBLOCK) {
		 if (!pipe->nreaders)
			err = -ENXIO;
	}
	else {
		while (err == EOK && !(pipe->nreaders && pipe->nwriters)) {
			err = pipe_wait(pipe);
		}
	}
	pipe_unlock(pipe);

	return err;
}


int npipe_open(file_t *file, int oflag)
{
	int err = EOK;
	oid_t oid;

#if 0
	file->status = oflag;

	oid.port = file->port->id;
	oid.id = file->id;

	/* TODO: better bump reference under common lock? */
	proc_lockSet(&pipe_common.lock);
	if ((file->pipe = npipe_find(&oid)) == NULL)
		file->pipe = npipe_create(&oid);
	proc_lockClear(&pipe_common.lock);

	if (file->pipe == NULL) {
		err = -ENOMEM;
	}
	else if ((err = pipe_open(&((named_pipe_t *)file->pipe)->pipe, oflag)) < 0) {
		npipe_destroy(file->pipe);
	}
#endif
	return err;
}
#endif