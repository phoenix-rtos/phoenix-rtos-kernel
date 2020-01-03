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
	int open;
} pipe_t;


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

	if (pipe->open) {
		if (!pipe->nreaders || !pipe->nwriters) {
			events |= POLLHUP;
		}
		else if (!fifo_is_full(&pipe->fifo)) {
			events |= POLLOUT;
		}
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
		return 1;
	}
	else {
		poll_signal(&pipe->wait, POLLHUP);
		return 0;
	}
}


int pipe_closeNamed(obdes_t *obdes, int read, int write)
{
	port_t *port;
	port = obdes->port;

	proc_lockSet(&port->odlock);
	if (pipe_close(obdes->pipe, read, write))
		obdes->pipe = NULL;
	proc_lockClear(&port->odlock);
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

	pipe->open = 0;
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
	iodes_t *read_end, *write_end;
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
	pipe->open = 1;

	read_end->status = (flags & ~O_CLOEXEC) | O_RDONLY;
	write_end->status = (flags & ~O_CLOEXEC) | O_WRONLY;

	write_end->pipe = read_end->pipe = pipe;
	write_end->type = read_end->type = ftPipe;

	if ((readfd = fd_new(process, 0, fdflags, read_end)) < 0) {
		file_put(read_end);
		file_put(write_end);
		pipe_destroy(pipe);
		vm_kfree(pipe);
		return readfd;
	}

	if ((writefd = fd_new(process, 0, fdflags, write_end)) < 0) {
		file_put(write_end);
		fd_close(process, readfd);
		pipe_destroy(pipe);
		vm_kfree(pipe);
		return writefd;
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


int pipe_get(obdes_t *obdes, pipe_t **result, int flags)
{
	port_t *port = obdes->port;
	pipe_t *pipe;
	int error;

	proc_lockSet(&port->odlock);
	if ((pipe = obdes->pipe) == NULL) {
		if ((pipe = vm_kmalloc(sizeof(*pipe))) == NULL) {
			proc_lockClear(&port->odlock);
			return -ENOMEM;
		}

		error = pipe_init(pipe, SIZE_PAGE);
		if (error < 0) {
			proc_lockClear(&port->odlock);
			vm_kfree(pipe);
			return error;
		}
		obdes->pipe = pipe;
	}

	if (flags & O_WRONLY) {
		lib_atomicIncrement(&pipe->nwriters);
	}
	else if (flags & O_RDWR) {
		lib_atomicIncrement(&pipe->nwriters);
		lib_atomicIncrement(&pipe->nreaders);
	}
	else {
		lib_atomicIncrement(&pipe->nreaders);
	}
	proc_lockClear(&port->odlock);

	*result = pipe;
	return EOK;
}


int pipe_open(pipe_t *pipe)
{
	int error;
	pipe_lock(pipe);
	if ((!pipe->nwriters || !pipe->nreaders)) {
		error = -EAGAIN;
	}
	else {
		pipe->open = 1;

		if (!fifo_is_full(&pipe->fifo)) {
			poll_signal(&pipe->wait, POLLOUT);
		}

		error = EOK;
	}
	pipe_unlock(pipe);

	return error;
}
