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
#include "../lib/lib.h"
#include "proc.h"

#define FD_HARD_LIMIT 1024

#define process_lock(p) proc_lockSet(&p->lock)
#define process_unlock(p) proc_lockClear(&p->lock)


struct _fildes_t {
	file_t *file;
	unsigned flags;
};


typedef struct _file_ops_t file_ops_t;


struct _file_t {
	unsigned refs;
	off_t offset;
	lock_t lock;
	mode_t mode;
	unsigned status;
	const file_ops_t *ops;
	oid_t oid;
};


struct _file_ops_t {
	ssize_t (*read)(file_t *, char *, size_t);
	ssize_t (*write)(file_t *, const char *, size_t);
	int (*close)(file_t *);
	int (*seek)(file_t *, off_t *, int);
	ssize_t (*setattr)(file_t *, int, const void *, size_t);
	ssize_t (*getattr)(file_t *, int, void *, size_t);
	int (*ioctl)(file_t *, pid_t, unsigned, void *);
	int (*link)(file_t *, const char *, const oid_t *);
	int (*unlink)(file_t *, const char *);
};


static struct {
	oid_t root;
	mode_t rootmode;
	spinlock_t lock;
} file_common;


mode_t file_root(oid_t *oid)
{
	mode_t result;

	hal_spinlockSet(&file_common.lock);
	hal_memcpy(oid, &file_common.root, sizeof(*oid));
	result = file_common.rootmode;
	hal_spinlockClear(&file_common.lock);
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
	return EOK;
}


static int generic_seek(file_t *file, long long *offset, int whence)
{
	return EOK;
}


static int generic_setattr(file_t *file, int attr, const char *value, ssize_t size)
{
	return proc_objectSetAttr(&file->oid, attr, value, size);
}


static ssize_t generic_getattr(file_t *file, int attr, char *value, ssize_t size)
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


static int generic_ioctl(file_t *file, pid_t pid, unsigned cmd, void *val)
{
	return EOK;
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


static int file_destroy(file_t *f)
{
	int error = EOK;

	if (f->ops != NULL)
		error = f->ops->close(f);

	proc_lockDone(&f->lock);
	vm_kfree(f);
	return error;
}


static void file_ref(file_t *f)
{
	lib_atomicIncrement(&f->refs);
}


static int file_put(file_t *f)
{
	int error = EOK;

	if (f && !lib_atomicDecrement(&f->refs))
		error = file_destroy(f);

	return error;
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


static int _fd_close(process_t *p, int fd, file_t **file)
{
	if (fd < 0 || fd >= p->fdcount || p->fds[fd].file == NULL)
		return -EBADF;

	*file = p->fds[fd].file;
	p->fds[fd].file = NULL;
	return EOK;
}


static int fd_close(process_t *p, int fd)
{
	int error;
	file_t *file;

	process_lock(p);
	error = _fd_close(p, fd, &file);
	process_unlock(p);

	if (!error)
		error = file_put(file);

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
	proc_lockSet(&f->lock);
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


int proc_fileLookup(oid_t *oid, mode_t *mode, const char *path, int flags, mode_t cmode)
{
	int err, sflags;
	const char *delim = path;

	cmode |= ((cmode & S_IFMT) == 0) * S_IFREG;

	do {
		path = delim + 1;

		while (*path && *path == '/')
			++path;

		delim = path;

		while (*delim && *delim != '/')
			delim++;

		if (path == delim)
			continue;

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

	return EOK;
}


static int _file_dup(process_t *p, int fd, int fd2, int flags)
{
	file_t *f, *f2;

	if (fd == fd2)
		return -EINVAL;

	if (fd2 < 0 || fd2 >= p->fdcount)
		return -EBADF;

	if ((f = _file_get(p, fd)) == NULL)
		return -EBADF;

	if (flags & FD_ALLOC) {
		if ((fd2 = _fd_alloc(p, fd2)) < 0) {
			file_put(f);
			return fd2;
		}

		flags &= ~FD_ALLOC;
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
		file_unlock(file);
		file_put(file);
	}

	if (dir != NULL)
		file_put(dir);

	return error < 0 ? error : fd;
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


int proc_fileIoctl(int fildes, unsigned long request, char *data)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	int retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	retval = file->ops->ioctl(file, process->id, request, data);
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


int proc_fileLink(int fildes, int dirfd, const char *name, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file, *dir;
	int retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	if ((dir = file_get(process, fildes)) == NULL) {
		file_put(file);
		return -EBADF;
	}

	if (!S_ISDIR(dir->mode)) {
		file_put(file);
		file_put(dir);
		return -ENOTDIR;
	}

	retval = dir->ops->link(dir, name, &file->oid);
	file_put(file);
	file_put(dir);
	return retval;
}


int proc_fileUnlink(int dirfd, const char *name, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *dir;
	int retval;

	if ((dir = file_get(process, dirfd)) == NULL)
		return -EBADF;

	if (!S_ISDIR(dir->mode)) {
		file_put(dir);
		return -ENOTDIR;
	}

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

	return flags;
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


int proc_fileStat(int fildes, file_stat_t *buf)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	ssize_t retval;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	if ((retval = file->ops->getattr(file, atStatStruct, (char *)buf, sizeof(*buf))) >= 0)
		retval = EOK;

	file_put(file);
	return retval;
}


int proc_fileChmod(int fildes, mode_t mode)
{
	return 0;
}


void proc_filesSetRoot(const oid_t *oid, mode_t mode)
{
	hal_spinlockSet(&file_common.lock);
	hal_memcpy(&file_common.root, oid, sizeof(*oid));
	file_common.rootmode = mode;
	hal_spinlockClear(&file_common.lock);
}


int proc_filesDestroy(process_t *process)
{
	int fd;
	file_t *file;

	process_lock(process);
	for (fd = 0; fd < process->fdcount; ++fd) {
		if (_fd_close(process, fd, &file) == EOK)
			file_put(file);
	}
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
	file_t *file;

	process_lock(process);
	for (fd = 0; fd < process->fdcount; ++fd) {
		if (process->fds[fd].file != NULL && process->fds[fd].flags & FD_CLOEXEC) {
			if (_fd_close(process, fd, &file) == EOK)
				file_put(file);
		}
	}
	process_unlock(process);

	return EOK;
}


void _file_init()
{
	file_common.root.port = 0;
	file_common.root.id = 0;
	hal_spinlockCreate(&file_common.lock, "file_root");
}

