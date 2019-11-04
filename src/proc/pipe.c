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
	thread_t *queue;
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


static int pipe_wait(pipe_t *pipe)
{
	return proc_lockWait(&pipe->queue, &pipe->lock, 0);
}


static void pipe_wakeup(pipe_t *pipe)
{
	if (pipe->queue != NULL)
		proc_threadBroadcastYield(&pipe->queue);
}


static int pipe_destroy(pipe_t *pipe)
{
	proc_lockDone(&pipe->lock);
	vm_kfree(pipe->fifo.data);
	return EOK;
}


static ssize_t pipe_invalid_read(file_t *file, void *data, size_t size, off_t offset)
{
	return -EBADF;
}


static ssize_t pipe_invalid_write(file_t *file, const void *data, size_t size, off_t offset)
{
	return -EBADF;
}


static ssize_t pipe_read(file_t *file, void *data, size_t size, off_t offset)
{
	ssize_t retval;
	pipe_t *pipe = file->data;

	pipe_lock(pipe);
	while ((retval = fifo_read(&pipe->fifo, data, size)) == 0) {
		if (!pipe->nwriters) {
			retval = 0;
			break;
		}
		if (file->status & O_NONBLOCK) {
			retval = -EWOULDBLOCK;
			break;
		}
		pipe_wait(pipe);
	}
	pipe_unlock(pipe);
	if (retval > 0)
		pipe_wakeup(pipe);
	return retval;
}


static ssize_t pipe_write(file_t *file, const void *data, size_t size, off_t offset)
{
	ssize_t retval = 0;
	pipe_t *pipe = file->data;
	int atomic;

	if (!pipe->nreaders)
		return -EPIPE;

	if (!size)
		return 0;

	pipe_lock(pipe);
	atomic = size <= fifo_size(&pipe->fifo);
	do {
		if (!pipe->nreaders) {
			retval = -EPIPE;
			break;
		}
		else if (fifo_freespace(&pipe->fifo) > atomic * (size - 1)) {
			retval += fifo_write(&pipe->fifo, data + retval, size - retval);
		}
		else if (file->status & O_NONBLOCK) {
			break;
		}
		else {
			pipe_wait(pipe);
		}
	} while (retval < size);
	pipe_unlock(pipe);
	if (retval > 0)
		pipe_wakeup(pipe);
	return retval ? retval : -EWOULDBLOCK;
}


static int pipe_close_read(file_t *file)
{
	pipe_t *pipe = file->data;
	if (!lib_atomicDecrement(&pipe->nreaders) && !pipe->nwriters) {
		pipe_destroy(pipe);
		vm_kfree(pipe);
	}
	else {
		pipe_wakeup(pipe);
	}
	return EOK;
}


static int pipe_close_write(file_t *file)
{
	pipe_t *pipe = file->data;
	if (!lib_atomicDecrement(&pipe->nwriters) && !pipe->nreaders) {
		pipe_destroy(pipe);
		vm_kfree(pipe);
	}
	else {
		pipe_wakeup(pipe);
	}
	return EOK;
}


static int pipe_invalid_seek(file_t *file, off_t *offset, int whence)
{
	return -ESPIPE;
}


static int pipe_setattr(file_t *file, int attr, const void *value, size_t size)
{
	lib_printf("pipe_setattr\n");
	return -ENOSYS;
}


static ssize_t pipe_getattr(file_t *file, int attr, void *value, size_t size)
{
	lib_printf("pipe_getattr\n");
	return -ENOSYS;
}


static int pipe_link(file_t *dir, const char *name, const oid_t *file)
{
	lib_printf("pipe_link\n");
	return -ENOTDIR;
}


static int pipe_unlink(file_t *dir, const char *name)
{
	lib_printf("pipe_unlink\n");
	return -ENOTDIR;
}


static int pipe_ioctl(file_t *file, unsigned cmd, const void *in_buf, size_t in_size, void *out_buf, size_t out_size)
{
	lib_printf("pipe_ioctl\n");
	return -ENOSYS;
}


const file_ops_t pipe_read_file_ops = {
	.read = pipe_read,
	.write = pipe_invalid_write,
	.release = pipe_close_read,
	.seek = pipe_invalid_seek,
	.setattr = pipe_setattr,
	.getattr = pipe_getattr,
	.link = pipe_link,
	.unlink = pipe_unlink,
	.ioctl = pipe_ioctl,
};


const file_ops_t pipe_write_file_ops = {
	.read = pipe_invalid_read,
	.write = pipe_write,
	.release = pipe_close_write,
	.seek = pipe_invalid_seek,
	.setattr = pipe_setattr,
	.getattr = pipe_getattr,
	.link = pipe_link,
	.unlink = pipe_unlink,
	.ioctl = pipe_ioctl,
};


int pipe_init(pipe_t *pipe, size_t size)
{
	if (!IS_POW_2(size))
		return -EINVAL;

	if ((pipe->fifo.data = vm_kmalloc(size)) == NULL)
		return -ENOMEM;

	pipe->nreaders = pipe->nwriters = 0;
	pipe->queue = NULL;
	proc_lockInit(&pipe->lock);
	fifo_init(&pipe->fifo, size);
	return EOK;
}


int pipe_create(process_t *process, size_t size, int fds[2], int flags)
{
	int read_fd = -1, write_fd = -1, err = EOK;
	pipe_t *pipe = NULL;

	if (!IS_POW_2(size)) {
		return -EINVAL;
	}
	else if ((pipe = vm_kmalloc(sizeof(pipe_t))) == NULL) {
		return -ENOMEM;
	}
	
	if ((err = pipe_init(pipe, size)) == EOK) {
		pipe->nreaders = 1;
		pipe->nwriters = 1;

		if ((read_fd = fd_create(process, 0, flags, 0, &pipe_read_file_ops, pipe)) < 0) {
			err = read_fd;
		}
		else if ((write_fd = fd_create(process, 0, flags, 0, &pipe_write_file_ops, pipe)) < 0) {
			fd_close(process, read_fd);
			err = write_fd;
		}
		else {
			fds[0] = read_fd;
			fds[1] = write_fd;
			return EOK;
		}

		pipe_destroy(pipe);
	}

	vm_kfree(pipe);
	return err;
}


int proc_pipeCreate(int fds[2], int flags)
{
	process_t *process = proc_current()->process;
	int retval;

	retval = pipe_create(process, SIZE_PAGE, fds, flags);
	return retval;
}


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
	named_pipe_t *npipe = file->data;
	pipe_t *pipe = &npipe->pipe;

	proc_lockSet(&pipe_common.lock);
	if (file->status & O_RDONLY) {
		lib_atomicDecrement(&pipe->nreaders);
	}
	if (file->status & O_WRONLY) {
		lib_atomicDecrement(&pipe->nwriters);
	}
	if (!pipe->nreaders && !pipe->nwriters) {
		npipe_destroy(file->data);
	}
	else {
		pipe_wakeup(pipe);
	}
	proc_lockClear(&pipe_common.lock);

	return EOK;
}


const file_ops_t npipe_ops = {
	.read = pipe_read,
	.write = pipe_write,
	.release = npipe_release,
	.seek = pipe_invalid_seek,
	.setattr = pipe_setattr,
	.getattr = pipe_getattr,
	.link = pipe_link,
	.unlink = pipe_unlink,
	.ioctl = pipe_ioctl,
};


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
	pipe_wakeup(pipe);
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

	file->ops = &npipe_ops;
	file->status = oflag;

	oid.port = file->port->id;
	oid.id = file->id;

	/* TODO: better bump reference under common lock? */
	proc_lockSet(&pipe_common.lock);
	if ((file->data = npipe_find(&oid)) == NULL)
		file->data = npipe_create(&oid);
	proc_lockClear(&pipe_common.lock);

	if (file->data == NULL) {
		err = -ENOMEM;
	}
	else if ((err = pipe_open(&((named_pipe_t *)file->data)->pipe, oflag)) < 0) {
		npipe_destroy(file->data);
	}

	return err;
}

