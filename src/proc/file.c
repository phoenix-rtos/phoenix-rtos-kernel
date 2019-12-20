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

#define LOOKUP_PARENT 1
#define LOOKUP_CREATE 2
#define LOOKUP_EXCLUSIVE 4

#define FILE_LOG(fmt, ...) lib_printf("%s:%d  %s(): " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)


enum { devNull, devZero };


typedef struct {
	const char *remaining;
	port_t *port;
	id_t id;
	mode_t mode;
	int flags;
} pathwalk_t;


static struct {
	lock_t lock;
	file_t *root;
	unsigned filecnt;
} file_common;


static void file_lock(file_t *f)
{
	proc_lockSet(&f->lock);
}


static void file_unlock(file_t *f)
{
	proc_lockClear(&f->lock);
}


static const char *file_basename(const char *path)
{
	int i;
	const char *name = path;

	for (i = 0; path[i]; ++i) {
		if (path[i] == '/')
			name = path + i + 1;
	}

	return name;
}


static ssize_t _file_read(file_t *file, void *data, size_t size, off_t offset)
{
	ssize_t retval = EOK;
	port_t *port = NULL;
	id_t id;

	switch (file->type) {
		case ftRegular:
		case ftDirectory: {
			port = file->fs.port;
			id = file->fs.id;
			break;
		}
		case ftSocket:
		case ftDevice: {
			port = file->device.port;
			id = file->device.id;
			break;
		}
		case ftPort: {
			FILE_LOG("got port");
			return -EOPNOTSUPP;
		}
		case ftPipe:
			break;
		case ftFifo:
			FILE_LOG("TODO");
			return -ENOSYS;
		default: {
			FILE_LOG("invalid file type");
			return -ENXIO;
		}
	}

	if (port == NULL && file->type == ftDevice) {
		switch (id) {
			case devNull:
				return 0;
			case devZero: {
				hal_memset(data, 0, size);
				return size;
			}
			default: {
				FILE_LOG("invalid special file: %lld", id);
				return -ENXIO;
			}
		}
	}
	else if (file->type == ftPipe || file->type == ftFifo) {
		retval = pipe_read(file->pipe, data, size);
	}
	else {
		retval = proc_objectRead(port, id, data, size, offset);
	}

	return retval;
}


ssize_t file_read(file_t *file, void *data, size_t size, off_t offset)
{
	ssize_t retval;
	file_lock(file);
	retval = _file_read(file, data, size, offset);
	file_unlock(file);
	return retval;
}


static ssize_t _file_write(file_t *file, const void *data, size_t size, off_t offset)
{
	ssize_t retval = EOK;
	port_t *port = NULL;
	id_t id;

	switch (file->type) {
		case ftRegular:
		case ftDirectory: {
			port = file->fs.port;
			id = file->fs.id;
			break;
		}
		case ftSocket:
		case ftDevice: {
			port = file->device.port;
			id = file->device.id;
			break;
		}
		case ftPort: {
			FILE_LOG("got port");
			return -EOPNOTSUPP;
		}
		case ftPipe:
			break;
		case ftFifo:{
			FILE_LOG("TODO");
			return -ENOSYS;
		}
		default: {
			FILE_LOG("invalid file type");
			return -ENXIO;
		}
	}

	if (port == NULL && file->type == ftDevice) {
		switch (id) {
			case devNull:
			case devZero:
				return size;
			default: {
				FILE_LOG("invalid special file: %lld", id);
				return -ENXIO;
			}
		}
	}
	else if (file->type == ftPipe || file->type == ftFifo) {
		retval = pipe_write(file->pipe, data, size);
	}
	else {
		retval = proc_objectWrite(port, id, data, size, offset);
	}

	return retval;
}


static int _file_release(file_t *file)
{
	int retval = EOK;

	switch (file->type) {
		case ftRegular:
		case ftDirectory: {
			retval = proc_objectClose(file->fs.port, file->fs.id);
			port_put(file->fs.port);
			break;
		}
		case ftSocket: {
			retval = proc_objectClose(file->device.port, file->device.id);
			port_put(file->device.port);
			break;
		}
		case ftDevice: {
			if (file->fs.port != NULL) {
				retval = proc_objectClose(file->fs.port, file->fs.id);
				port_put(file->fs.port);
			}
			if (file->device.port != NULL) {
				retval = proc_objectClose(file->device.port, file->device.id);
				port_put(file->device.port);
			}
			break;
		}
		case ftPort: {
			port_put(file->port);
			retval = EOK;
			break;
		}
		case ftFifo: {
			retval = proc_objectClose(file->fs.port, file->fs.id);
			port_put(file->fs.port);
			/* fallthrough */
		}
		case ftPipe: {
			pipe_close(file->pipe, (file->status & O_RDONLY) || (file->status & O_RDWR), (file->status & O_WRONLY) || (file->status & O_RDWR));
			break;
		}
		default: {
			FILE_LOG("invalid file type");
			retval = -ENXIO;
			break;
		}
	}

	return retval;
}


static int _file_seek(file_t *file, off_t *offset, int whence)
{
	int retval = EOK;
	size_t size;

	switch (file->type) {
		case ftFifo:
		case ftPipe:
		case ftPort:
		case ftSocket:
			return -ESPIPE;
	}

	switch (whence) {
		case SEEK_SET: {
			file->offset = *offset;
			break;
		}
		case SEEK_END: {
			if ((retval = proc_objectGetAttr(file->fs.port, file->fs.id, atSize, &size, sizeof(size))) == sizeof(size)) {
				file->offset = size + *offset;
				*offset = file->offset;
				retval = EOK;
			}
			else if (retval >= 0) {
				FILE_LOG("getting size, gettattr=%d", retval);
				retval = -EIO;
			}
			else {
				FILE_LOG("getting size");
			}
			break;
		}
		case SEEK_CUR: {
			file->offset += *offset;
			*offset = file->offset;
			break;
		}
		default:
			retval = -EINVAL;
	}

	return retval;
}


void file_destroy(file_t *f)
{
	if (f->obdes != NULL)
		port_obdesPut(f->obdes);

	proc_lockDone(&f->lock);
	vm_kfree(f);
}


void file_ref(file_t *f)
{
	lib_atomicIncrement(&f->refs);
}


int file_put(file_t *f)
{
	if (f && !lib_atomicDecrement(&f->refs)) {
		_file_release(f);
		file_destroy(f);
	}

	return EOK;
}


file_t *file_alloc(void)
{
	file_t *file;

	if ((file = vm_kmalloc(sizeof(*file))) != NULL) {
		hal_memset(file, 0, sizeof(*file));
		file->refs = 1;
		proc_lockInit(&file->lock);
	}

	return file;
}


static int file_poll(file_t *file, poll_head_t *poll, wait_note_t *note)
{
	int revents = 0;
	obdes_t *od;

	if ((od = file->obdes) != NULL) {
		poll_add(poll, &od->queue, note);

		if ((proc_objectGetAttr(od->port, od->id, atEvents, &revents, sizeof(revents))) < 0) {
			FILE_LOG("getattr");
			revents = POLLERR;
		}

		return revents;
	}

	switch (file->type) {
		case ftFifo:
		case ftPipe: {
			revents = pipe_poll(file->pipe, poll, note);
			break;
		}
		default: {
			FILE_LOG("TODO type %d", file->type);
			revents = POLLNVAL;
			break;
		}
	}
	return revents;
}


int file_waitForOne(file_t *file, int events, int timeout)
{
	poll_head_t poll;
	wait_note_t note;
	obdes_t *obdes = file->obdes;
	int revents = 0;
	int err = EOK;

	events |= POLLERR | POLLNVAL | POLLHUP;

	poll_init(&poll);
	revents = file_poll(file, &poll, &note);

	poll_lock();
	revents |= note.events;
	while (!(revents & events)) {
		_poll_wait(&poll, timeout);
		revents |= note.events;
	}
	_poll_remove(&note);
	poll_unlock();
	return err;
}


int proc_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms)
{
	int nev = 0, i, events, error = EOK, revents;
	wait_note_t snotes[16];
	wait_note_t *notes;
	poll_head_t poll;
	file_t *file;
	obdes_t *obdes;
	process_t *process = proc_current()->process;

	if (!timeout_ms)
		timeout_ms = -1;
	else if (timeout_ms < 0)
		timeout_ms = 0;

	if (nfds <= 16) {
		notes = snotes;
	}
	else if ((notes = vm_kmalloc(nfds * sizeof(wait_note_t))) == NULL) {
		return -ENOMEM;
	}

	hal_memset(notes, 0, nfds * sizeof(notes[0]));
	poll_init(&poll);

	for (i = 0; i < nfds; ++i) {
		if (fds[i].fd < 0)
			continue;

		file = file_get(process, fds[i].fd);
		if (file == NULL) {
			FILE_LOG("bad fd %d", fds[i].fd);
			fds[i].revents = POLLNVAL;
			nev++;
			continue;
		}

		events = file_poll(file, &poll, notes + i);

		if ((revents = (fds[i].events | POLLERR | POLLHUP) & events)) {
			fds[i].revents = revents;
			nev++;
		}
		file_put(file);
	}

	poll_lock();
	if (!nev) {
		error = EOK;
		for (;;) {
			for (i = 0; i < nfds; ++i) {
				if ((revents = notes[i].events & (fds[i].events | POLLERR | POLLHUP))) {
					fds[i].revents = revents;
					nev++;
				}
			}

			if (nev || error || timeout_ms < 0)
				break;

			if ((error = _poll_wait(&poll, timeout_ms)) == -EINTR)
				poll_lock();
		}
	}

	for (i = 0; i < nfds; ++i)
		_poll_remove(notes + i);
	poll_unlock();

	if (nfds > 16) {
		vm_kfree(notes);
	}

	return nev ? nev : error;
}


///////////////////////// FIXME: can't usually share root file! investigate
static file_t *file_root(void)
{
	file_t *root;
	proc_lockSet(&file_common.lock);
	if ((root = file_common.root) != NULL)
		file_ref(root);
	proc_lockClear(&file_common.lock);
	return root;
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


int fd_close(process_t *p, int fd)
{
	int error;
	process_lock(p);
	error = _fd_close(p, fd);
	process_unlock(p);
	return error;
}


static int _fd_new(process_t *p, int minfd, unsigned flags, file_t *file)
{
	int fd;

	if ((fd = _fd_alloc(p, minfd)) < 0)
		return fd;

	p->fds[fd].file = file;
	p->fds[fd].flags = flags;
	return fd;
}


static file_t *_file_get(process_t *p, int fd)
{
	file_t *f;

	if (fd < 0 || fd >= p->fdcount || (f = p->fds[fd].file) == NULL)
		return NULL;

	file_ref(f);
	return f;
}


int fd_new(process_t *p, int fd, int flags, file_t *file)
{
	int retval;
	process_lock(p);
	retval = _fd_new(p, fd, flags, file);
	process_unlock(p);
	return retval;
}


file_t *file_get(process_t *p, int fd)
{
	file_t *f;
	process_lock(p);
	f = _file_get(p, fd);
	process_unlock(p);
	return f;
}


int file_followMount(port_t **port, id_t *id)
{
	int error;
	oid_t dest;
	port_t *newport;

	if ((error = proc_objectRead(*port, *id, &dest, sizeof(oid_t), 0)) != sizeof(oid_t)) {
		FILE_LOG("read device node %u.%llu", (*port)->id, *id);
		asm volatile ("1: jmp 1b");
		return error;
	}

	if (dest.port == 0) {
		/* kernel-handled device, not a directory */
		return -ENOTDIR;
	}

	if (dest.port == (*port)->id) {
		/* probably should not happen */
		FILE_LOG("same filesystem?");
		asm volatile ("1: jmp 1b");
	}

	if ((newport = port_get(dest.port)) == NULL) {
		FILE_LOG("get port (%d)", dest.port);
		return -ENXIO;
	}

	if ((error = proc_objectOpen(newport, &dest.id)) != EOK) {
		FILE_LOG("open");
		port_put(newport);
		return error;
	}

	/* TODO: maybe getattr to check if it's a directory? */

	if ((error = proc_objectClose(*port, *id)) != EOK) {
		FILE_LOG("close");
		/* don't fail as we have the mounted fs opened already */
	}

	port_put(*port);
	*port = newport;
	*id = dest.id;
	return EOK;
}


int file_walkPath(pathwalk_t *state)
{
	int err = EOK;
	const char *delim = state->remaining, *path;
	port_t *port = state->port;
	id_t nextid, id = state->id;

	if (state->remaining == NULL)
		return EOK;

	if (state->port == NULL) {
		FILE_LOG("given NULL port");
		return -EINVAL;
	}

	path = delim;

	do {
		while (*path && *path == '/')
			++path;

		delim = path;

		while (*delim && *delim != '/')
			delim++;

		if (path == delim)
			continue;

		if (!*delim && (state->flags & LOOKUP_PARENT))
			break;

		if (!S_ISDIR(state->mode)) {
			err = -ENOTDIR;
			break;
		}

		if ((err = proc_objectLookup(port, id, path, delim - path, 0, &nextid, &state->mode, NULL)) < 0) {
			break;
		}

		proc_objectClose(port, id);
		id = nextid;
		path = delim;
	} while (*delim);

	state->id = id;
	state->remaining = *path ? path : NULL;
	return err;
}


int file_walkLast(pathwalk_t *state, mode_t mode, oid_t *dev)
{
	int pathlen = hal_strlen(state->remaining);
	int err, flags = 0;
	id_t newid;

	if (state->flags & LOOKUP_CREATE)
		flags |= O_CREAT;
	if (state->flags & LOOKUP_EXCLUSIVE)
		flags |= O_EXCL;

	if ((err = proc_objectLookup(state->port, state->id, state->remaining, pathlen, flags, &newid, &mode, dev)) < 0) {
		return err;
	}

	proc_objectClose(state->port, state->id);
	state->id = newid;
	state->mode = mode;
	state->remaining = NULL;
	return EOK;
}


int file_followPath(pathwalk_t *walk)
{
	int err;

	while ((err = file_walkPath(walk)) == -ENOTDIR) {
		if (S_ISLNK(walk->mode)) {
			/* TODO: handle symlink */
			asm volatile ("1: jmp 1b");
			FILE_LOG("symlink");
			err = -ENOSYS;
			break;
		}
		else if (S_ISMNT(walk->mode)) {
			if ((err = file_followMount(&walk->port, &walk->id)) < 0)
				break;
			walk->mode = (walk->mode & ~S_IFMT) | S_IFDIR;
		}
		else {
			break;
		}
	}

	return err;
}


int file_fsOpen(port_t **port, id_t *id, const char *path, int flags, mode_t *mode, oid_t *dev)
{
	int err;
	pathwalk_t walk;

	walk.port = *port;
	walk.id = *id;
	walk.remaining = path;
	walk.flags = 0;
	walk.mode = S_IFDIR;

	if ((flags & O_CREAT) | (flags & O_PARENT))
		walk.flags |= LOOKUP_PARENT;

	err = file_followPath(&walk);

	if (err == EOK && (flags & O_CREAT)) {
		walk.flags &= ~LOOKUP_PARENT;
		walk.flags |= LOOKUP_CREATE;

		if (flags & O_EXCL)
			walk.flags |= LOOKUP_EXCLUSIVE;

		err = file_walkLast(&walk, *mode, dev);
	}

	*port = walk.port;
	*id = walk.id;
	*mode = walk.mode;

	return err;
}


int file_openBase(file_t **result, process_t *process, int fd, const char *path)
{
	file_t *base = NULL;

	if (path == NULL)
		return -EINVAL;

	if (path[0] != '/') {
		if (fd == AT_FDCWD) {
			/* FIXME: keep process locked? - race against chdir() */
			if ((base = process->cwd) == NULL) {
				FILE_LOG("current directory not set");
				return -ENOENT;
			}
			file_ref(process->cwd);
		}
		else if ((base = file_get(process, fd)) == NULL) {
			return -EBADF;
		}

		if (base->type != ftDirectory) {
			file_put(base);
			return -ENOTDIR;
		}
	}
	else {
		if ((base = file_root()) == NULL) {
			FILE_LOG("no root filesystem");
			return -ENOENT;
		}
	}

	*result = base;
	return EOK;
}


int file_copyFs(file_t *dst, file_t *src)
{
	int error;

	dst->fs.port = src->fs.port;
	dst->fs.id = src->fs.id;
	port_ref(dst->fs.port);

	if ((error = proc_objectOpen(dst->fs.port, &dst->fs.id)) < 0) {
		FILE_LOG("open = %d", error);
		return error;
	}

	return EOK;
}


int file_openDevice(file_t *file)
{
	int error;
	oid_t dest;
	port_t *port;

	if ((error = proc_objectRead(file->fs.port, file->fs.id, &dest, sizeof(oid_t), 0)) != sizeof(oid_t)) {
		FILE_LOG("read device node");
		return error;
	}

	if (dest.port == 0) {
		port = NULL;
		switch (dest.id) {
			case devNull:
			case devZero:
				break;
			default: {
				FILE_LOG("invalid kernel device");
				return -ENXIO;
			}
		}
	}
	else {
		if ((port = port_get(dest.port)) == NULL) {
			FILE_LOG("get port (%d)", dest.port);
			return -ENXIO;
		}

		if ((error = proc_objectOpen(port, &dest.id)) != EOK) {
			FILE_LOG("open");
			port_put(port);
			return error;
		}
	}

	file->device.port = port;
	file->device.id = dest.id;
	return EOK;
}


int file_open(file_t **result, process_t *process, int dirfd, const char *path, int flags, mode_t mode)
{
	int error = EOK;
	file_t *dir = NULL, *file;
	mode_t open_mode;
	size_t size;

	if ((flags & O_TRUNC) && (flags & O_RDONLY))
		return -EINVAL;

	if ((error = file_openBase(&dir, process, dirfd, path)) < 0)
		return error;

	if ((file = file_alloc()) == NULL) {
		file_put(dir);
		return -ENOMEM;
	}

	error = file_copyFs(file, dir);
	file_put(dir);

	if (error < 0)
		return error;

	open_mode = mode;
	if ((error = file_fsOpen(&file->fs.port, &file->fs.id, path, flags, &open_mode, NULL)) < 0) {
		proc_objectClose(file->fs.port, file->fs.id);
		port_put(file->fs.port);
		return error;
	}

	if ((flags & O_DIRECTORY) && !S_ISMNT(open_mode) && !S_ISDIR(open_mode)) {
		file_put(file);
		return -ENOTDIR;
	}

	switch (open_mode & S_IFMT) {
		case S_IFREG: {
			file->type = ftRegular;
			file->obdes = port_obdesGet(file->fs.port, file->fs.id);
			break;
		}
		case S_IFDIR: {
			file->type = ftDirectory;
			file->obdes = port_obdesGet(file->fs.port, file->fs.id);
			break;
		}
		case S_IFBLK:
		case S_IFCHR: {
			file->type = ftDevice;
			file_openDevice(file);
			if (file->device.port != NULL) {
				file->obdes = port_obdesGet(file->device.port, file->device.id);
			}
			else {
				file->obdes = NULL;
			}
			break;
		}
		case S_IFMNT: {
			file->type = ftDirectory;
			file_followMount(&file->fs.port, &file->fs.id);
			file->obdes = port_obdesGet(file->fs.port, file->fs.id);
			break;
		}
		case S_IFSOCK: {
			file->type = ftSocket;
			FILE_LOG("TODO");
			error = -ENOSYS;
			break;
		}
		case S_IFIFO: {
			file->type = ftFifo;
			FILE_LOG("no fifos for now");
			error = -ENOSYS;
			break;
		}
		case S_IFLNK: {
			FILE_LOG("symlinks not supported yet");
			error = -ENOSYS;
			break;
		}
		default: {
			FILE_LOG("file type not recognized");
			error = -ENXIO;
			break;
		}
	}

	if (error < 0) {
		file_put(file);
	}
	else {
		if ((flags & O_TRUNC) && file->obdes != NULL) {
			size = 0;
			proc_objectSetAttr(file->obdes->port, file->obdes->id, atSize, &size, sizeof(size));
		}

		*result = file;
	}

	return error;
}


int file_resolve(file_t **file, process_t *process, int fildes, const char *path, int flags)
{
	int err;
	if (path == NULL) {
		if ((*file = file_get(process, fildes)) == NULL)
			err = -EBADF;
	}
	else {
		err = file_open(file, process, fildes, path, flags, 0);
	}
	return err;
}


static int _file_dup(process_t *p, int fd, int fd2, int flags)
{
	file_t *f, *f2;

	if (fd == fd2)
		return -EINVAL;

	if (fd2 < 0 || (f = _file_get(p, fd)) == NULL)
		return -EBADF;

	if (flags & FD_ALLOC || fd2 >= p->fdcount) {
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


static file_t *file_setRoot(file_t *newroot)
{
	file_t *oldroot;
	proc_lockSet(&file_common.lock);
	oldroot = file_common.root;
	file_common.root = newroot;
	proc_lockClear(&file_common.lock);
	return oldroot;
}


int proc_filesSetRoot(int fd, id_t id, mode_t mode)
{
	file_t *root, *port;
	process_t *process = proc_current()->process;

	if ((port = file_get(process, fd)) == NULL) {
		FILE_LOG("bad file descriptor");
		return -EBADF;
	}

	if (port->type != ftPort) {
		FILE_LOG("given descriptor is not a port");
		file_put(port);
		return -EBADF;
	}

	if ((root = file_alloc()) == NULL) {
		file_put(port);
		return -ENOMEM;
	}

	if (port->port == NULL)
		asm volatile ("1: jmp 1b");

	root->type = ftDirectory; /* TODO: check */
	root->fs.port = port->port;
	root->fs.id = id;

	file_put(file_setRoot(root));
	return EOK;
}


int proc_fileOpen(int dirfd, const char *path, int flags, mode_t mode)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	int error = EOK;

	/* FIXME - we currently use this to create directories too.. */
	if (!(mode & S_IFMT))
		mode |= S_IFREG;

	if ((error = file_open(&file, process, dirfd, path, flags, mode)) < 0)
		return error;

	return fd_new(process, 0, 0, file);
}


/* TODO: get rid of this */
int proc_fileOid(process_t *process, int fd, oid_t *oid)
{
	int retval = -EBADF;
	file_t *file;

	process_lock(process);
	if ((file = _file_get(process, fd)) != NULL) {
		retval = EOK;
		switch (file->type) {
			case ftRegular:
			case ftDirectory: {
				oid->port = file->fs.port != NULL ? file->fs.port->id : 0;
				oid->id = file->fs.id;
				break;
			}
			case ftDevice:
			case ftSocket: {
				oid->port = file->device.port->id;
				oid->id = file->device.id;
				break;
			}
			default: {
				FILE_LOG("oid for type (%d)", file->type);
				retval = -EBADF;
				break;
			}
		}
		file_put(file);
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
	int err;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	for (;;) {
		file_lock(file);
		if ((retval = _file_read(file, buf, nbyte, file->offset)) > 0)
			file->offset += retval;
		file_unlock(file);

		if ((file->status & O_NONBLOCK) || retval != -EAGAIN)
			break;

		if ((retval = file_waitForOne(file, POLLIN, 0)) < 0)
			break;
	}

	file_put(file);
	return retval;
}


ssize_t proc_fileWrite(int fildes, const char *buf, size_t nbyte)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	file_t *file;
	ssize_t retval;
	int err;

	if ((file = file_get(process, fildes)) == NULL)
		return -EBADF;

	for (;;) {
		file_lock(file);
		if ((retval = _file_write(file, buf, nbyte, file->offset)) > 0)
			file->offset += retval;
		file_unlock(file);

		if ((file->status & O_NONBLOCK) || retval != -EAGAIN)
			break;

		if ((retval = file_waitForOne(file, POLLOUT, 0)) < 0)
			break;
	}

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

	file_lock(file);
	retval = _file_seek(file, offset, whence);
	file_unlock(file);
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

	file_lock(file);
	switch (file->type) {
		case ftRegular: {
			retval = proc_objectSetAttr(file->fs.port, file->fs.id, atSize, &length, sizeof(length));
			break;
		}
		case ftDevice: {
			if (file->device.port == NULL) {
				retval = EOK;
			}
			else {
				retval = proc_objectSetAttr(file->device.port, file->device.id, atSize, &length, sizeof(length));
			}
			break;
		}
		case ftDirectory: {
			retval = -EISDIR;
			break;
		}
		default: {
			/* TODO: what else to return? */
			retval = -ESPIPE;
			break;
		}
	}
	file_unlock(file);
	file_put(file);

	if (retval != sizeof(length) && retval >= 0) {
		FILE_LOG("setattr");
		retval = -EIO;
	}

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

	file_lock(file);
	switch (file->type) {
		case ftRegular:
		case ftDirectory: {
			retval = -ENOTTY;
			break;
		}
		case ftFifo:
		case ftPipe: {
			retval = pipe_ioctl(file->pipe, request, indata, insz, outdata, outsz);
			break;
		}
		case ftSocket:
		case ftDevice: {
			if (file->device.port != NULL) {
				retval = proc_objectControl(file->device.port, file->device.id, request, indata, insz, outdata, outsz);
			}
			else {
				FILE_LOG("TODO");
				retval = -ENOTTY;
			}
			break;
		}
		default: {
			FILE_LOG("invalid file type");
			retval = -ENXIO;
			break;
		}
	}
	file_unlock(file);
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
	oid_t oid;

	if ((retval = file_resolve(&dir, process, dirfd, name, O_DIRECTORY | O_PARENT)) < 0)
		return retval;

	linkname = file_basename(name);

	if ((retval = file_resolve(&file, process, fildes, path, 0)) < 0) {
		file_put(dir);
		return retval;
	}

	oid.port = file->fs.port->id;
	oid.id = file->fs.id;

	file_lock(dir);
	retval = proc_objectLink(dir->fs.port, dir->fs.id, linkname, &oid);
	file_unlock(dir);

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
	const char *linkname;

	if ((retval = file_open(&dir, process, dirfd, path, O_DIRECTORY | O_PARENT, 0)) < 0)
		return retval;

	linkname = file_basename(path);

	file_lock(dir);
	retval = proc_objectUnlink(dir->fs.port, dir->fs.id, linkname);
	file_unlock(dir);

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
	port_t *port;
	id_t id;
	mode_t mode = 0;

	if (path == NULL) {
		if ((file = file_get(process, fildes)) == NULL)
			return -EBADF;
	}
	else if ((err = file_openBase(&file, process, fildes, path)) < 0) {
		return err;
	}

	if (file->fs.port == NULL) {
		file_put(file);
		return -EBADF;
	}

	port = file->fs.port;
	id = file->fs.id;

	if (path != NULL) {
		port_ref(port);
		proc_objectOpen(port, &id);
		if ((err = file_fsOpen(&port, &id, path, 0, &mode, NULL)) < 0) {
			port_put(port);
			file_put(file);
			return err;
		}
	}

	if ((err = proc_objectGetAttr(port, id, atStatStruct, (char *)buf, sizeof(*buf))) >= 0)
		err = EOK;

	if (path != NULL) {
		proc_objectClose(port, id);
		port_put(port);
	}

	file_put(file);
	return err;
}


int proc_fileChmod(int fildes, mode_t mode)
{
	return 0;
}


int proc_filesDestroy(process_t *process)
{
	int fd;

	process_lock(process);
	for (fd = 0; fd < process->fdcount; ++fd)
		_fd_close(process, fd);
	vm_kfree(process->fds);
	process->fds = NULL;
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
	else {
		process->cwd = file_root();
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


int proc_fifoCreate(int dirfd, const char *path, mode_t mode)
{
	process_t *process = proc_current()->process;
	int err, pplen;
	file_t *dir;
	const char *fifoname;
	id_t id;

	/* only permission bits allowed */
	if (mode & ~(S_IRWXU | S_IRWXG | S_IRWXO))
		return -EINVAL;

	if ((err = file_resolve(&dir, process, dirfd, path, O_PARENT | O_DIRECTORY)) < 0)
		return err;

	fifoname = file_basename(path);
	mode |= S_IFIFO;

	if ((err = proc_objectLookup(dir->fs.port, dir->fs.id, fifoname, hal_strlen(fifoname), O_CREAT | O_EXCL, &id, &mode, NULL)) == EOK)
		proc_objectClose(dir->port, id);
	file_put(dir);
	return err;
}


int proc_deviceCreate(int dirfd, const char *path, int portfd, id_t id, mode_t mode)
{
	process_t *process = proc_current()->process;
	int err, pplen;
	file_t *dir, *port = NULL;
	const char *name;
	oid_t oid;

	if ((err = file_resolve(&dir, process, dirfd, path, O_PARENT | O_DIRECTORY)) < 0) {
		FILE_LOG("dir resolve");
		return err;
	}

	oid.id = id;

	if (portfd == -1) {
		oid.port = 0;
	}
	else if ((port = file_get(process, portfd)) == NULL || (port->type != ftPort)) {
		FILE_LOG("port");
		file_put(dir);
		file_put(port);
		return -EBADF;
	}
	else {
		oid.port = port->port->id;
	}

	name = file_basename(path);

	if ((err = proc_objectLookup(dir->fs.port, dir->fs.id, name, hal_strlen(name), O_CREAT | O_EXCL, &id, &mode, &oid)) == EOK)
		proc_objectClose(dir->fs.port, id);
	else
		FILE_LOG("lookup");

	file_put(dir);
	file_put(port);
	return err;
}


int proc_changeDir(int fildes, const char *path)
{
	process_t *process = proc_current()->process;
	file_t *file;
	int retval;

	if ((retval = file_resolve(&file, process, fildes, path, O_DIRECTORY)) < 0)
		return retval;

	process_lock(process);
	if (process->cwd != NULL)
		file_put(process->cwd);
	process->cwd = file;
	process_unlock(process);
	return EOK;
}


int proc_fsMount(int devfd, const char *devpath, const char *type, unsigned portno)
{
	process_t *process = proc_current()->process;
	int retval;
	file_t *device, *root;
	id_t rootid;
	mode_t mode;
	port_t *port;

	if ((retval = file_resolve(&device, process, devfd, devpath, 0)) < 0) {
		FILE_LOG("resolve");
		return retval;
	}

	if ((retval = port_create(&port, portno)) < 0) {
		FILE_LOG("could not create port %d", portno);
		return retval;
	}

	retval = proc_objectMount(device->device.port, device->device.id, port->id, type, &rootid, &mode);
	file_put(device);

	if (retval < 0) {
		port_put(port);
		FILE_LOG("mount failed");
		return retval;
	}

	if (!S_ISDIR(mode)) {
		port_put(port);
		FILE_LOG("bad mode of mounted directory");
		return -ENOTDIR;
	}

	if ((root = file_alloc()) == NULL) {
		port_put(port);
		return -ENOMEM;
	}

	root->type = ftDirectory;
	root->fs.port = port;
	root->fs.id = rootid;

	if ((retval = fd_new(process, 0, 0, root)) < 0)
		file_put(root);

	return retval;
}


int proc_fsBind(int dirfd, const char *dirpath, int fsfd, const char *fspath)
{
	process_t *process = proc_current()->process;
	file_t *dir, *fs;
	oid_t oid;
	int retval;

	if (dirpath[0] == '/' && dirpath[1] == 0) {
		dir = NULL;
		oid.port = 0;
		oid.id = 0;
	}
	else {
		if ((retval = file_resolve(&dir, process, dirfd, dirpath, O_DIRECTORY)) < 0)
			return retval;

		oid.port = dir->fs.port->id;
		oid.id = dir->fs.id;
	}

	if ((retval = file_resolve(&fs, process, fsfd, fspath, O_DIRECTORY)) < 0) {
		file_put(dir);
		return retval;
	}

	if (dir != NULL) {

		FILE_LOG("binding dir=%u.%llu to fs=%u.%llu", dir->fs.port->id, dir->fs.id, fs->fs.port->id, fs->fs.id);

		retval = proc_objectSetAttr(fs->fs.port, fs->fs.id, atMountPoint, &oid, sizeof(oid));

		if (retval == EOK) {
			oid.port = fs->fs.port->id;
			oid.id = fs->fs.id;
			retval = proc_objectSetAttr(dir->fs.port, dir->fs.id, atMount, &oid, sizeof(oid));
		}
	}
	else {
		fs = file_setRoot(fs);
	}

	file_put(dir);
	file_put(fs);
	return retval;
}


/* Sockets */

static int socket_get(process_t *process, int socket, file_t **file)
{
	if ((*file = file_get(process, socket)) == NULL)
		return -EBADF;

	if ((*file)->type != ftSocket) {
		file_put(*file);
		return -ENOTSOCK;
	}

	return EOK;
}


int proc_netAccept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags)
{
	file_t *file, *conn;
	process_t *process = proc_current()->process;
	int retval;
	id_t conn_id;
	int block, err;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	block = !(file->status & O_NONBLOCK);

	do {
		if ((retval = proc_objectAccept(file->device.port, file->device.id, &conn_id, address, address_len)) == EOK) {
			if ((conn = file_alloc()) != NULL) {
				conn->device.port = file->device.port;
				port_ref(file->device.port);
				conn->device.id = conn_id;
				conn->type = ftSocket;
				conn->obdes = port_obdesGet(conn->device.port, conn_id);

				if ((retval = fd_new(process, 0, flags, conn)) < 0)
					file_put(conn);
			}
			else {
				proc_objectClose(file->device.port, conn_id);
				retval = -ENOMEM;
			}
		}
		else if (retval == -EAGAIN) {
			if ((err = file_waitForOne(file, POLLIN, 0)) < 0)
				retval = err;
		}
	}
	while (block && retval == -EAGAIN);

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

	retval = proc_objectBind(file->device.port, file->device.id, address, address_len);
	file_put(file);
	return retval;
}


int proc_netConnect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	file_t *file;
	process_t *process = proc_current()->process;
	int retval, block, submitted = 0, err;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	block = !(file->status & O_NONBLOCK);

	do {
		if ((retval = proc_objectConnect(file->device.port, file->device.id, address, address_len)) == -EINPROGRESS && block) {
			submitted = 1;
			if ((err = file_waitForOne(file, POLLOUT, 0)) < 0)
				retval = err;
		}
		else if (retval == -EALREADY && submitted) {
			retval = EOK;
		}
	}
	while (block && retval == -EINPROGRESS);

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

	if ((retval = proc_objectGetAttr(file->device.port, file->device.id, atRemoteAddr, address, *address_len)) > 0)
		*address_len = retval;

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

	if ((retval = proc_objectGetAttr(file->device.port, file->device.id, atLocalAddr, address, *address_len)) > 0)
		*address_len = retval;

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

	retval = -EINVAL; //socket_getsockopt(file->port, file->id, level, optname, optval, optlen);
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

	retval = proc_objectListen(file->device.port, file->device.id, backlog);
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

	retval = -EINVAL; //socket_recvfrom(file->port, file->id, message, length, flags, src_addr, src_len);
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

	retval = -EINVAL; //socket_sendto(file->port, file->id, message, length, flags, dest_addr, dest_len);
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

	retval = proc_objectShutdown(file->device.port, file->device.id, how);
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

	retval = -EINVAL; //socket_setsockopt(file->port, file->id, level, optname, optval, optlen);
	file_put(file);
	return retval;
}


int proc_netSocket(int domain, int type, int protocol)
{
	process_t *process = proc_current()->process;
	file_t *file;
	int err;

	if ((err = file_open(&file, proc_current()->process, -1, "/dev/net", 0, 0)) != EOK)
		return err;

	file->type = ftSocket;

	if (err != EOK || (err = fd_new(process, 0, 0, file)) < 0)
		file_put(file);

	return err;
}


void _file_init()
{
	file_common.root = NULL;
	proc_lockInit(&file_common.lock);
}
