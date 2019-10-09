/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * File
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/types.h"
#include "../../include/errno.h"
#include "../../include/fcntl.h"
#include "../../include/ioctl.h"
#include "../../include/socket.h"
#include "../../include/event.h"
#include "../lib/lib.h"
#include "proc.h"
#include "socket.h"
#include "event.h"

#define FD_HARD_LIMIT 1024
#define IS_POW_2(x) ((x) && !((x) & ((x) - 1)))

struct _fildes_t {
	file_t *file;
	unsigned flags;
};


typedef struct _pipe_t {
	lock_t lock;
	fifo_t fifo;
	thread_t *queue;
	unsigned nreaders;
	unsigned nwriters;
} pipe_t;


static struct {
	lock_t lock;
	oid_t root;
	mode_t rootmode;
	rbtree_t vfiles;
} file_common;


mode_t file_root(oid_t *oid)
{
	mode_t result;

	proc_lockSet(&file_common.lock);
	hal_memcpy(oid, &file_common.root, sizeof(*oid));
	result = file_common.rootmode;
	proc_lockClear(&file_common.lock);
	return result;
}


static ssize_t generic_read(file_t *file, void *data, size_t size)
{
	ssize_t retval;

	if ((retval = proc_objectRead(&file->oid, data, size, file->offset)) > 0)
		file->offset += retval;

	return retval;
}


static ssize_t generic_write(file_t *file, const void *data, size_t size)
{
	ssize_t retval;

	if ((retval = proc_objectWrite(&file->oid, data, size, file->offset)) > 0)
		file->offset += retval;

	return retval;
}


static int generic_close(file_t *file)
{
	return proc_objectClose(&file->oid);
}


static int generic_seek(file_t *file, off_t *offset, int whence)
{
	return EOK;
}


static int generic_setattr(file_t *file, int attr, const void *value, size_t size)
{
	return proc_objectSetAttr(&file->oid, attr, value, size);
}


static ssize_t generic_getattr(file_t *file, int attr, void *value, size_t size)
{
	return proc_objectGetAttr(&file->oid, attr, value, size);
}


static int generic_link(file_t *dir, const char *name, const oid_t *file)
{
	return proc_objectLink(&dir->oid, name, file);
}


static int generic_unlink(file_t *dir, const char *name)
{
	return proc_objectUnlink(&dir->oid, name);
}


static int generic_ioctl(file_t *file, unsigned cmd, const void *in_buf, size_t in_size, void *out_buf, size_t out_size)
{
	return proc_objectControl(&file->oid, cmd, in_buf, in_size, out_buf, out_size);
}


const file_ops_t generic_file_ops = {
	.read = generic_read,
	.write = generic_write,
	.close = generic_close,
	.seek = generic_seek,
	.setattr = generic_setattr,
	.getattr = generic_getattr,
	.link = generic_link,
	.unlink = generic_unlink,
	.ioctl = generic_ioctl,
};


/* file_t functions */

static void file_lock(file_t *f)
{
	proc_lockSet(&f->lock);
}


static void file_unlock(file_t *f)
{
	proc_lockClear(&f->lock);
}


static void file_destroy(file_t *f)
{
	if (f->ops != NULL)
		f->ops->close(f);
	proc_lockDone(&f->lock);
	vm_kfree(f);
}


static void file_ref(file_t *f)
{
	lib_atomicIncrement(&f->refs);
}


static int file_put(file_t *f)
{
	if (f && !lib_atomicDecrement(&f->refs))
		file_destroy(f);

	return EOK;
}


static file_t *file_alloc(file_t *orig)
{
	file_t *file;

	if ((file = vm_kmalloc(sizeof(*file))) != NULL) {
		file->refs = 1;
		file->offset = 0;
		file->status = 0;
		proc_lockInit(&file->lock);

		if (orig != NULL) {
			hal_memcpy(&file->oid, &orig->oid, sizeof(oid_t));
			file->mode = orig->mode;
			file->ops = orig->ops;
		}
		else {
			file->ops = &generic_file_ops;
			file->mode = file_root(&file->oid);
		}
	}

	return file;
}


/* File descriptor table functions */

static int _fd_realloc(process_t *p)
{
	fildes_t *new;
	int fdcount;

	fdcount = p->fdcount ? p->fdcount * 2 : 4;

	if (fdcount > FD_HARD_LIMIT)
		return -ENFILE;

	if ((new = vm_kmalloc(fdcount * sizeof(fildes_t))) == NULL)
		return -ENOMEM;

	hal_memcpy(new, p->fds, p->fdcount * sizeof(fildes_t));
	hal_memset(new + p->fdcount, 0, (fdcount - p->fdcount) * sizeof(fildes_t));

	vm_kfree(p->fds);
	p->fds = new;
	p->fdcount = fdcount;

	return EOK;
}


static int _fd_alloc(process_t *p, int fd)
{
	int err = EOK;

	while (err == EOK) {
		while (fd < p->fdcount) {
			if (p->fds[fd].file == NULL)
				return fd;

			fd++;
		}

		err = _fd_realloc(p);
	}

	return err;
}


static int _fd_close(process_t *p, int fd)
{
	file_t *file;
	int error = EOK;

	if (fd < 0 || fd >= p->fdcount || (file = p->fds[fd].file) == NULL)
		return -EBADF;
	p->fds[fd].file = NULL;
	file_put(file);

	return error;
}


static int fd_close(process_t *p, int fd)
{
	int error;
	process_lock(p);
	error = _fd_close(p, fd);
	process_unlock(p);
	return error;
}


static int _file_new(process_t *p, int minfd, file_t **file)
{
	file_t *f;
	int fd;

	if ((fd = _fd_alloc(p, minfd)) < 0)
		return fd;

	if ((*file = f = p->fds[fd].file = vm_kmalloc(sizeof(file_t))) == NULL)
		return -ENOMEM;

	hal_memset(f, 0, sizeof(file_t));
	proc_lockInit(&f->lock);
	f->refs = 2;
	f->offset = 0;
	f->mode = 0;
	f->status = 0;
	f->ops = NULL;

	return fd;
}


static file_t *_file_get(process_t *p, int fd)
{
	file_t *f;

	if (fd < 0 || fd >= p->fdcount || (f = p->fds[fd].file) == NULL || f->ops == NULL)
		return NULL;

	file_ref(f);
	return f;
}


static int file_new(process_t *p, int fd, file_t **file)
{
	int retval;
	process_lock(p);
	retval = _file_new(p, fd, file);
	process_unlock(p);
	return retval;
}


static file_t *file_get(process_t *p, int fd)
{
	file_t *f;
	process_lock(p);
	f = _file_get(p, fd);
	process_unlock(p);
	return f;
}


static int file_followLink(oid_t *oid, mode_t *mode)
{
	return -ENOSYS;
}


int proc_fileLookup(oid_t *oid, mode_t *mode, const char *name, int flags, mode_t cmode)
{
	int err, sflags, ret = EOK;
	const char *delim = name, *path;

	cmode |= ((cmode & S_IFMT) == 0) * S_IFREG;

	do {
		path = delim;

		while (*path && *path == '/')
			++path;

		delim = path;

		while (*delim && *delim != '/')
			delim++;

		if (path == delim)
			continue;

		if (!*delim && (flags & O_PARENT)) {
			ret = path - name;
			break;
		}

		if (S_ISLNK(*mode) && (err = file_followLink(oid, mode)) < 0)
			return err;
		else if (S_ISMNT(*mode) && (err = proc_objectRead(oid, (char *)oid, sizeof(*oid), 0)) < 0)
			return err;
		else if (!S_ISDIR(*mode))
			return -ENOTDIR;

		*mode = cmode;
		sflags = *delim ? 0 : flags;

		if ((err = proc_objectLookup(oid, path, delim - path, sflags, &oid->id, mode)) < 0)
			return err;

	} while (*delim);

	if ((flags & O_DIRECTORY) && !S_ISDIR(*mode))
		return -ENOTDIR;
	else if (S_ISLNK(*mode) && (err = file_followLink(oid, mode)) < 0)
		return err;
	else if (!S_ISDIR(*mode) && !S_ISREG(*mode) && !(flags & O_NOFOLLOW) && (err = proc_objectRead(oid, (char *)oid, sizeof(oid_t), 0)) < 0)
		return err;

	return ret;
}


static int file_resolve(process_t *process, int fildes, const char *path, int flags, file_t **result)
{
	file_t *file, *dir;
	int err;

	if (flags & (O_CREAT | O_TRUNC | O_EXCL))
		return -EINVAL;

	if (path != NULL && path[0] == '/') {
		if ((file = file_alloc(NULL)) == NULL)
			return -ENOMEM;
	}
	else {
		if (fildes == AT_FDCWD) {
			process_lock(process);
			file = process->cwd;
			file_ref(file);
			process_unlock(process);
		}
		else if ((file = file_get(process, fildes)) == NULL) {
			return -EBADF;
		}

		if (path == NULL) {
			*result = file;
			return EOK;
		}

		dir = file;
		if (!S_ISDIR(dir->mode)) {
			file_put(file);
			return -ENOTDIR;
		}

		file = file_alloc(dir);
		file_put(dir);
		if (file == NULL)
			return -ENOMEM;
	}

	if ((err = proc_fileLookup(&file->oid, &file->mode, path, flags, 0)) >= 0)
		*result = file;

	return EOK;
}


int proc_fileResolve(process_t *process, int fildes, const char *path, int flags, oid_t *oid)
{
	file_t *file;
	int err;

	if ((err = file_resolve(process, fildes, path, flags, &file)) < 0)
		return err;

	hal_memcpy(oid, &file->oid, sizeof(oid_t));
	return err;
}


static int _file_dup(process_t *p, int fd, int fd2, int flags)
{
	file_t *f, *f2;

	if (fd == fd2)
		return -EINVAL;

	if (fd2 < 0 || (f = _file_get(p, fd)) == NULL)
		return -EBADF;

	if (flags & FD_ALLOC) {
		if ((fd2 = _fd_alloc(p, fd2)) < 0) {
			file_put(f);
			return fd2;
		}

		flags &= ~FD_ALLOC;
	}
	else if (fd2 >= p->fdcount) {
		file_put(f);
		return -EBADF;
	}
	else if ((f2 = p->fds[fd2].file) != NULL) {
		file_put(f2);
	}

	p->fds[fd2].file = f;
	p->fds[fd2].flags = flags;
	return fd2;
}


int proc_fileOpen(int dirfd, const char *path, int flags, mode_t mode)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	int error = 0, fd;
	file_t *dir = NULL, *file;

	if (path[0] != '/') {
		if ((dir = file_get(process, dirfd)) == NULL)
			return -EBADF;

		if (!S_ISDIR(dir->mode)) {
			file_put(dir);
			return -ENOTDIR;
		}
	}

	if ((fd = file_new(process, 0, &file)) >= 0) {
		if (dir != NULL) {
			hal_memcpy(&file->oid, &dir->oid, sizeof(oid_t));
			file->mode = dir->mode;
		}
		else {
			file->mode = file_root(&file->oid);
		}

		if ((error = proc_fileLookup(&file->oid, &file->mode, path, flags, mode)) < 0)
			fd_close(process, fd);

		file->ops = &generic_file_ops;
		file_put(file);
	}

	if (dir != NULL)
		file_put(dir);

	return error < 0 ? error : fd;
}

int proc_fileOid(process_t *process, int fd, oid_t *oid)
{
	int retval = -EBADF;
	file_t *file;

	process_lock(process);
	if ((file = _file_get(process, fd)) != NULL) {
		hal_memcpy(oid, &file->oid, sizeof(oid_t));
		file_put(file);
		retval = EOK;
	}
	process_unlock(process);
	return retval;
}


int proc_fileClose(int fildes)
{
	thread_t *current = proc_current();
	process_t *process = current->process;

	return fd_close(process, fildes);
}


ssize_t proc_fileRead(int fildes, char *buf, size_t nbyte)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	ssize_t retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	retval = file->ops->read(file, buf, nbyte);
	file_put(file);
	return retval;
}


ssize_t proc_fileWrite(int fildes, const char *buf, size_t nbyte)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	ssize_t retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	retval = file->ops->write(file, buf, nbyte);
	file_put(file);
	return retval;
}


int proc_fileSeek(int fildes, off_t *offset, int whence)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	int retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	retval = file->ops->seek(file, offset, whence);
	file_put(file);
	return retval;
}


int proc_fileTruncate(int fildes, off_t length)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	ssize_t retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	if ((retval = file->ops->setattr(file, atSize, &length, sizeof(length))) >= 0)
		retval = EOK;

	file_put(file);
	return retval;
}


int proc_fileIoctl(int fildes, unsigned long request, const char *indata, size_t insz, char *outdata, size_t outsz)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	int retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	retval = file->ops->ioctl(file, request, indata, insz, outdata, outsz);
	file_put(file);
	return retval;
}


int proc_fileDup(int fildes, int fildes2, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	int retval;

	process_lock(process);
	retval = _file_dup(process, fildes, fildes2, flags);
	process_unlock(process);
	return retval;
}


int proc_fileLink(int fildes, const char *path, int dirfd, const char *name, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file, *dir;
	int retval;
	const char *linkname;

	if ((retval = file_resolve(process, dirfd, name, O_DIRECTORY | O_PARENT, &dir)) < 0)
		return retval;

	linkname = name + retval;

	if ((retval = file_resolve(process, fildes, path, 0, &file)) < 0) {
		file_put(dir);
		return retval;
	}

	retval = dir->ops->link(dir, linkname, &file->oid);

	file_put(file);
	file_put(dir);
	return retval;
}


int proc_fileUnlink(int dirfd, const char *path, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *dir;
	int retval;
	const char *name;

	if ((retval = file_resolve(process, dirfd, path, O_PARENT | O_DIRECTORY, &dir)) < 0)
		return retval;

	name = path + retval;

	retval = dir->ops->unlink(dir, name);
	file_put(dir);
	return retval;
}


static int fcntl_getFd(int fd)
{
	process_t *p = proc_current()->process;
	file_t *file;
	int flags;

	process_lock(p);
	if ((file = _file_get(p, fd)) == NULL) {
		process_unlock(p);
		return -EBADF;
	}

	flags = p->fds[fd].flags;
	process_unlock(p);
	file_put(file);

	return flags;
}


static int fcntl_setFd(int fd, int flags)
{
	process_t *p = proc_current()->process;
	file_t *file;

	process_lock(p);
	if ((file = _file_get(p, fd)) == NULL) {
		process_unlock(p);
		return -EBADF;
	}

	p->fds[fd].flags = flags;
	process_unlock(p);
	file_put(file);

	return EOK;
}


static int fcntl_getFl(int fd)
{
	process_t *p = proc_current()->process;
	file_t *file;
	int status;

	if ((file = file_get(p, fd)) == NULL)
		return -EBADF;

	status = file->status;
	file_put(file);

	return status;
}


static int fcntl_setFl(int fd, int val)
{
	process_t *p = proc_current()->process;
	file_t *file;
	int ignored = O_CREAT|O_EXCL|O_NOCTTY|O_TRUNC|O_RDONLY|O_RDWR|O_WRONLY;

	if ((file = file_get(p, fd)) == NULL)
		return -EBADF;

	file_lock(file);
	file->status = (val & ~ignored) | (file->status & ignored);
	file_unlock(file);
	file_put(file);

	return 0;
}


int proc_fileControl(int fildes, int cmd, long arg)
{
	int err, flags = 0;

	switch (cmd) {
	case F_DUPFD_CLOEXEC:
		flags = FD_CLOEXEC;
		/* fallthrough */
	case F_DUPFD:
		err = proc_fileDup(fildes, arg, flags | FD_ALLOC);
		break;

	case F_GETFD:
		err = fcntl_getFd(fildes);
		break;

	case F_SETFD:
		err = fcntl_setFd(fildes, arg);
		break;

	case F_GETFL:
		err = fcntl_getFl(fildes);
		break;

	case F_SETFL:
		err = fcntl_setFl(fildes, arg);
		break;

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		err = EOK;
		break;

	case F_GETOWN:
	case F_SETOWN:
	default:
		err = -EINVAL;
		break;
	}

	return err;
}


int proc_fileStat(int fildes, const char *path, file_stat_t *buf, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	int err;

	if ((err = file_resolve(process, fildes, path, flags, &file)) != EOK)
		return err;

	if ((err = file->ops->getattr(file, atStatStruct, (char *)buf, sizeof(*buf))) >= 0)
		err = EOK;

	file_put(file);
	return err;
}


int proc_fileChmod(int fildes, mode_t mode)
{
	return 0;
}


int proc_filesSetRoot(const oid_t *oid, mode_t mode)
{
	proc_lockSet(&file_common.lock);
	hal_memcpy(&file_common.root, oid, sizeof(*oid));
	file_common.rootmode = mode;
	proc_lockClear(&file_common.lock);
	return EOK;
}


int proc_filesDestroy(process_t *process)
{
	int fd;

	process_lock(process);
	for (fd = 0; fd < process->fdcount; ++fd)
		_fd_close(process, fd);
	process_unlock(process);

	return EOK;
}


static int _proc_filesCopy(process_t *parent)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	int fd;

	if (process->fdcount)
		return -EINVAL;

	if ((process->fds = vm_kmalloc(parent->fdcount * sizeof(fildes_t))) == NULL)
		return -ENOMEM;

	process->fdcount = parent->fdcount;
	hal_memcpy(process->fds, parent->fds, parent->fdcount * sizeof(fildes_t));

	for (fd = 0; fd < process->fdcount; ++fd) {
		if (process->fds[fd].file != NULL)
			file_ref(process->fds[fd].file);
	}

	if (parent->cwd != NULL) {
		process->cwd = parent->cwd;
		file_ref(process->cwd);
	}
	/* FIXME */
	else if (S_ISDIR(file_common.rootmode)) {
		process->cwd = file_alloc(NULL);
	}
	return EOK;
}


int proc_filesCopy(process_t *parent)
{
	int rv;
	process_lock(parent);
	rv = _proc_filesCopy(parent);
	process_unlock(parent);
	return rv;
}


int proc_filesCloseExec(process_t *process)
{
	int fd;

	process_lock(process);
	for (fd = 0; fd < process->fdcount; ++fd) {
		if (process->fds[fd].file != NULL && (process->fds[fd].flags & FD_CLOEXEC))
			_fd_close(process, fd);
	}
	process_unlock(process);

	return EOK;
}


/* pipes */

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
	vm_kfree(pipe);
	return EOK;
}


static ssize_t pipe_invalid_read(file_t *file, void *data, size_t size)
{
	return -EBADF;
}


static ssize_t pipe_invalid_write(file_t *file, const void *data, size_t size)
{
	return -EBADF;
}


static ssize_t pipe_read(file_t *file, void *data, size_t size)
{
	ssize_t retval;
	pipe_t *pipe = file->pipe;

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


static ssize_t pipe_write(file_t *file, const void *data, size_t size)
{
	ssize_t retval = 0;
	pipe_t *pipe = file->pipe;
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
	pipe_t *pipe = file->pipe;
	if (!lib_atomicDecrement(&pipe->nreaders) && !pipe->nwriters)
		pipe_destroy(pipe);
	else
		pipe_wakeup(pipe);
	return EOK;
}


static int pipe_close_write(file_t *file)
{
	pipe_t *pipe = file->pipe;
	if (!lib_atomicDecrement(&pipe->nwriters) && !pipe->nreaders)
		pipe_destroy(pipe);
	else
		pipe_wakeup(pipe);
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
	.close = pipe_close_read,
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
	.close = pipe_close_write,
	.seek = pipe_invalid_seek,
	.setattr = pipe_setattr,
	.getattr = pipe_getattr,
	.link = pipe_link,
	.unlink = pipe_unlink,
	.ioctl = pipe_ioctl,
};


int pipe_create(process_t *process, size_t size, int fds[2])
{
	int read_fd = -1, write_fd = -1, err = EOK;
	file_t *write_file = NULL, *read_file = NULL;
	pipe_t *pipe = NULL;

	if (!IS_POW_2(size)) {
		err = -EINVAL;
	}
	else if ((pipe = vm_kmalloc(sizeof(pipe_t))) == NULL) {
		err = -ENOMEM;
	}
	else if ((pipe->fifo.data = vm_kmalloc(size)) == NULL) {
		err = -ENOMEM;
	}
	else if ((write_fd = _file_new(process, 0, &write_file)) < 0) {
		err = write_fd;
	}
	else if ((read_fd = _file_new(process, 0, &read_file)) < 0) {
		err = read_fd;
	}
	else {
		pipe->nreaders = 1;
		pipe->nwriters = 1;
		pipe->queue = NULL;
		proc_lockInit(&pipe->lock);
		fifo_init(&pipe->fifo, size);

		write_file->mode = S_IFIFO;
		write_file->pipe = pipe;
		write_file->ops = &pipe_write_file_ops;

		read_file->mode = S_IFIFO;
		read_file->pipe = pipe;
		read_file->ops = &pipe_read_file_ops;

		file_put(read_file);
		file_put(write_file);

		fds[0] = read_fd;
		fds[1] = write_fd;
		return EOK;
	}

	if (pipe != NULL) {
		vm_kfree(pipe->fifo.data);
		vm_kfree(pipe);
	}

	file_put(read_file);
	file_put(write_file);
	_fd_close(process, read_fd);
	_fd_close(process, write_fd);
	return err;
}


int proc_pipeCreate(int fds[2])
{
	process_t *process = proc_current()->process;
	int retval;

	process_lock(process);
	retval = pipe_create(process, SIZE_PAGE, fds);
	process_unlock(process);
	return retval;
}


/* Named pipes */

/* Event queue */


static ssize_t queue_read(file_t *file, void *data, size_t size)
{
	return -EINVAL;
}


static ssize_t queue_write(file_t *file, const void *data, size_t size)
{
	return -EINVAL;
}


static int queue_close(file_t *file)
{
	return -EINVAL; /* TODO */
}


static int queue_seek(file_t *file, off_t *offset, int whence)
{
	return -EINVAL;
}


static int queue_setattr(file_t *file, int attr, const void *value, size_t size)
{
	return -EINVAL;
}


static ssize_t queue_getattr(file_t *file, int attr, void *value, size_t size)
{
	return -EINVAL;
}


static int queue_link(file_t *dir, const char *name, const oid_t *file)
{
	return -EINVAL;
}


static int queue_unlink(file_t *dir, const char *name)
{
	return -EINVAL;
}


static int queue_ioctl(file_t *file, unsigned cmd, const void *in_buf, size_t in_size, void *out_buf, size_t out_size)
{
	return -EINVAL;
}


const file_ops_t queue_ops = {
	.read = queue_read,
	.write = queue_write,
	.close = queue_close,
	.seek = queue_seek,
	.setattr = queue_setattr,
	.getattr = queue_getattr,
	.link = queue_link,
	.unlink = queue_unlink,
	.ioctl = queue_ioctl,
};


int proc_queueCreate(void)
{
	process_t *process = proc_current()->process;
	evqueue_t *queue;
	file_t *file;
	int fd;

	if ((queue = queue_create(process)) == NULL)
		return -ENOMEM;

	process_lock(process);
	if ((fd = _file_new(process, 0, &file)) >= 0) {
		file->mode = S_IFEVQ;
		file->queue = queue;
		file->ops = &queue_ops;
		process->fds[fd].flags = FD_CLOEXEC;
		file_put(file);
	}
	process_unlock(process);
	return fd;
}


int proc_queueWait(int fd, const struct _event_t *subs, int subcnt, struct _event_t *events, int evcnt, time_t timeout)
{
	file_t *file;
	process_t *process = proc_current()->process;

	if ((file = file_get(process, fd)) == NULL)
		return -EBADF;

	if (!S_ISEVTQ(file->mode)) {
		file_put(file);
		return -EBADF;
	}

	return queue_wait(file->queue, subs, subcnt, events, evcnt, timeout);
}


/* Sockets */

static int socket_get(process_t *process, int socket, file_t **file)
{
	if ((*file = file_get(process, socket)) == NULL)
		return -EBADF;

	if (!S_ISSOCK((*file)->mode)) {
		file_put(*file);
		return -ENOTSOCK;
	}

	return EOK;
}


int proc_netAccept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_accept(&file->oid, address, address_len, flags);
	file_put(file);
	return retval;
}


int proc_netBind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_bind(&file->oid, address, address_len);
	file_put(file);
	return retval;
}


int proc_netConnect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_connect(&file->oid, address, address_len);
	file_put(file);
	return retval;
}


int proc_netGetpeername(int socket, struct sockaddr *address, socklen_t *address_len)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_getpeername(&file->oid, address, address_len);
	file_put(file);
	return retval;
}


int proc_netGetsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_getsockname(&file->oid, address, address_len);
	file_put(file);
	return retval;
}


int proc_netGetsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_getsockopt(&file->oid, level, optname, optval, optlen);
	file_put(file);
	return retval;
}


int proc_netListen(int socket, int backlog)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_listen(&file->oid, backlog);
	file_put(file);
	return retval;
}


ssize_t proc_netRecvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	file_t *file;
	process_t *process = proc_current()->process;
	ssize_t retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_recvfrom(&file->oid, message, length, flags, src_addr, src_len);
	file_put(file);
	return retval;
}


ssize_t proc_netSendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	file_t *file;
	process_t *process = proc_current()->process;
	ssize_t retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_sendto(&file->oid, message, length, flags, dest_addr, dest_len);
	file_put(file);
	return retval;
}


int proc_netShutdown(int socket, int how)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_shutdown(&file->oid, how);
	file_put(file);
	return retval;
}


int proc_netSetsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	retval = socket_setsockopt(&file->oid, level, optname, optval, optlen);
	file_put(file);
	return retval;
}


int proc_netSocket(int domain, int type, int protocol)
{
	process_t *process = proc_current()->process;
	file_t *file;
	int sockfd, err;

	if ((sockfd = file_new(process, 0, &file)) < 0)
		return sockfd;

	file->mode = S_IFSOCK;

	switch (domain) {
	case AF_INET:
		if ((err = socket_create(&file->oid, domain, type, protocol)) >= 0) {
			file->ops = &generic_file_ops;
			err = sockfd;
		}
		break;
	default:
		err = -EAFNOSUPPORT;
		break;
	}

	if (err < 0) {
		fd_close(process, sockfd);
	}

	file_put(file);
	return err;
}


void _file_init()
{
	file_common.root.port = 0;
	file_common.root.id = 0;
	proc_lockInit(&file_common.lock);
}

