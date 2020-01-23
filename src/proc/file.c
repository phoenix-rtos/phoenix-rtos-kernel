/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * File
 *
 * Copyright 2019, 2020 Phoenix Systems
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
#include "sun.h"

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
	iodes_t *root;
	unsigned filecnt;
} file_common;


static void file_lock(iodes_t *f)
{
	proc_lockSet(&f->lock);
}


static void file_unlock(iodes_t *f)
{
	proc_lockClear(&f->lock);
}


const char *file_basename(const char *path)
{
	int i;
	const char *name = path;

	for (i = 0; path[i]; ++i) {
		if (path[i] == '/')
			name = path + i + 1;
	}

	return name;
}


static ssize_t _file_read(iodes_t *file, void *data, size_t size, off_t offset)
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
		case ftLocalSocket:
			return sun_read(file->sun, data, size);
		case ftFifo:
		case ftPipe:
			return pipe_read(file->pipe, data, size);
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
	else {
		retval = proc_objectRead(port, id, data, size, offset);
	}

	return retval;
}


ssize_t file_read(iodes_t *file, void *data, size_t size, off_t offset)
{
	ssize_t retval;
	file_lock(file);
	retval = _file_read(file, data, size, offset);
	file_unlock(file);
	return retval;
}


static ssize_t _file_write(iodes_t *file, const void *data, size_t size, off_t offset)
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
		case ftLocalSocket:
			return sun_write(file->sun, data, size);
		case ftFifo:
		case ftPipe:
			return pipe_write(file->pipe, data, size);
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
	else {
		retval = proc_objectWrite(port, id, data, size, offset);
	}

	return retval;
}


static int _file_release(iodes_t *file)
{
	int retval = EOK, r, w;

	if (file->status & O_WRONLY) {
		r = 0; w = 1;
	}
	else if (file->status & O_RDWR) {
		r = 1; w = 1;
	}
	else {
		r = 1; w = 0;
	}

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
		case ftLocalSocket: {
			retval = EOK;
			if (file->sun != NULL) {
				retval = sun_close(file->sun);
			}
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
			pipe_closeNamed(file->obdes, r, w);
			retval = proc_objectClose(file->fs.port, file->fs.id);
			port_put(file->fs.port);
			break;
		}
		case ftPipe: {
			pipe_close(file->pipe, r, w);
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


static int _file_seek(iodes_t *file, off_t *offset, int whence)
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


void file_destroy(iodes_t *f)
{
	if (f->obdes != NULL)
		port_obdesPut(f->obdes);

	proc_lockDone(&f->lock);
	vm_kfree(f);
}


void file_ref(iodes_t *f)
{
	lib_atomicIncrement(&f->refs);
}


int file_put(iodes_t *f)
{
	if (f && !lib_atomicDecrement(&f->refs)) {
		_file_release(f);
		file_destroy(f);
	}

	return EOK;
}


iodes_t *file_alloc(void)
{
	iodes_t *file;

	if ((file = vm_kmalloc(sizeof(*file))) != NULL) {
		hal_memset(file, 0, sizeof(*file));
		file->refs = 1;
		proc_lockInit(&file->lock);
	}

	return file;
}


static int file_poll(iodes_t *file, poll_head_t *poll, wait_note_t *note)
{
	int revents = 0;
	obdes_t *od;

	switch (file->type) {
		case ftFifo:
		case ftPipe: {
			revents = pipe_poll(file->pipe, poll, note);
			break;
		}
		case ftLocalSocket: {
			revents = sun_poll(file->sun, poll, note);
			break;
		}
		default: {
			if ((od = file->obdes) != NULL) {
				poll_add(poll, &od->queue, note);

				if ((proc_objectGetAttr(od->port, od->id, atEvents, &revents, sizeof(revents))) < 0) {
					FILE_LOG("getattr");
					revents = POLLERR;
				}
			}
			else {
				FILE_LOG("TODO type %d", file->type);
				revents = POLLNVAL;
			}
			break;
		}
	}
	return revents;
}


int file_waitForOne(iodes_t *file, int events, int timeout)
{
	poll_head_t poll;
	wait_note_t note;
	int revents = 0;
	int err = EOK;

	events |= POLLERR | POLLNVAL | POLLHUP;

	poll_init(&poll);
	revents = file_poll(file, &poll, &note);

	poll_lock();
	revents |= note.events;
	while (!(revents & events)) {
		if ((err = _poll_wait(&poll, timeout)) < 0)
			break;
		revents |= note.events;
	}
	_poll_remove(&note);
	poll_unlock();
	return err;
}


int proc_poll(struct pollfd *handles, nfds_t nfds, int timeout_ms)
{
	int nev = 0, i, events, error = EOK, revents;
	wait_note_t snotes[16];
	wait_note_t *notes;
	poll_head_t poll;
	iodes_t *file;
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
		if (handles[i].fd < 0)
			continue;

		file = file_get(process, handles[i].fd);
		if (file == NULL) {
			FILE_LOG("bad handle %d", handles[i].fd);
			handles[i].revents = POLLNVAL;
			nev++;
			continue;
		}

		events = file_poll(file, &poll, notes + i);

		if ((revents = (handles[i].events | POLLERR | POLLHUP) & events)) {
			handles[i].revents = revents;
			nev++;
		}
		file_put(file);
	}

	poll_lock();
	if (!nev) {
		error = EOK;
		for (;;) {
			for (i = 0; i < nfds; ++i) {
				if ((revents = notes[i].events & (handles[i].events | POLLERR | POLLHUP))) {
					handles[i].revents = revents;
					nev++;
				}
				else {
					handles[i].revents = 0;
				}
			}

			if (nev || error || timeout_ms < 0)
				break;

			error = _poll_wait(&poll, timeout_ms);
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


static iodes_t *file_root(void)
{
	iodes_t *root;
	proc_lockSet(&file_common.lock);
	if ((root = file_common.root) != NULL)
		file_ref(root);
	proc_lockClear(&file_common.lock);
	return root;
}


/* File descriptor table functions */

static int _fd_realloc(process_t *p)
{
	hades_t *new;
	int hcount;

	hcount = p->hcount ? p->hcount * 2 : 4;

	if (hcount > FD_HARD_LIMIT)
		return -ENFILE;

	if ((new = vm_kmalloc(hcount * sizeof(hades_t))) == NULL)
		return -ENOMEM;

	hal_memcpy(new, p->handles, p->hcount * sizeof(hades_t));
	hal_memset(new + p->hcount, 0, (hcount - p->hcount) * sizeof(hades_t));

	vm_kfree(p->handles);
	p->handles = new;
	p->hcount = hcount;

	return EOK;
}


static int _fd_alloc(process_t *p, int handle)
{
	int err = EOK;

	while (err == EOK) {
		while (handle < p->hcount) {
			if (p->handles[handle].file == NULL)
				return handle;

			handle++;
		}

		err = _fd_realloc(p);
	}

	return err;
}


static int _fd_close(process_t *p, int handle)
{
	iodes_t *file;
	int error = EOK;

	if (handle < 0 || handle >= p->hcount || (file = p->handles[handle].file) == NULL)
		return -EBADF;

	p->handles[handle].file = NULL;

	if (HD_TYPE(p->handles[handle].flags) == hades_io)
		file_put(file);

	return error;
}


int fd_close(process_t *p, int handle)
{
	int error;
	process_lock(p);
	error = _fd_close(p, handle);
	process_unlock(p);
	return error;
}


static int _fd_new(process_t *p, int minfd, unsigned flags, iodes_t *file)
{
	int handle;

	if ((handle = _fd_alloc(p, minfd)) < 0)
		return handle;

	p->handles[handle].file = file;
	p->handles[handle].flags = flags;
	return handle;
}


static iodes_t *_file_get(process_t *p, int handle)
{
	iodes_t *f;

	if (handle < 0 || handle >= p->hcount || (f = p->handles[handle].file) == NULL || HD_TYPE(p->handles[handle].flags) != hades_io)
		return NULL;

	file_ref(f);
	return f;
}


int fd_new(process_t *p, int handle, int flags, iodes_t *file)
{
	int retval;
	process_lock(p);
	retval = _fd_new(p, handle, flags, file);
	process_unlock(p);
	return retval;
}


iodes_t *file_get(process_t *p, int handle)
{
	iodes_t *f;
	process_lock(p);
	f = _file_get(p, handle);
	process_unlock(p);
	return f;
}


int file_followMount(port_t **port, id_t *id)
{
	int error;
	oid_t dest;
	port_t *newport;

	if ((error = proc_objectGetAttr(*port, *id, atMount, &dest, sizeof(oid_t))) < 0) {
		FILE_LOG("read mounted fs %u.%llu = %d", (*port)->id, *id, error);
		return error;
	}

	if (dest.port == 0) {
		/* kernel-handled device, not a directory */
		return -ENOTDIR;
	}

	if (dest.port == (*port)->id) {
		/* probably should not happen */
		FILE_LOG("same filesystem?");
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

		if (!S_ISDIR(state->mode)) {
			err = -ENOTDIR;
			break;
		}

		if (!*delim && (state->flags & LOOKUP_PARENT))
			break;

		if ((err = proc_objectLookup(port, id, path, delim - path, 0, &nextid, &state->mode, NULL)) < 0) {
			break;
		}

		proc_objectClose(port, id);
		id = nextid;
		path = delim;
	} while (*delim);

	state->id = id;
	state->remaining = *path ? path : NULL;

	/* We will follow the mount later */
	if (S_ISMNT(state->mode))
		err = -ENOTDIR;

	return err;
}


int file_walkLast(pathwalk_t *state, mode_t mode, oid_t *dev)
{
	int pathlen;
	int err, flags = 0;
	id_t newid;

	if (state->remaining == NULL) {
		if (state->flags & (LOOKUP_CREATE | LOOKUP_EXCLUSIVE) == (LOOKUP_CREATE | LOOKUP_EXCLUSIVE))
			return -EEXIST;
		else
			return EOK;
	}

	pathlen = hal_strlen(state->remaining);

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


int file_openBase(iodes_t **result, process_t *process, int handle, const char *path)
{
	iodes_t *base = NULL;

	if (path == NULL)
		return -EINVAL;

	if (path[0] != '/') {
		if (handle == AT_FDCWD) {
			/* FIXME: keep process locked? - race against chdir() */
			if ((base = process->cwd) == NULL) {
				FILE_LOG("current directory not set");
				return -ENOENT;
			}
			file_ref(process->cwd);
		}
		else if ((base = file_get(process, handle)) == NULL) {
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


int file_copyFs(iodes_t *dst, iodes_t *src)
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


int file_openDevice(iodes_t *file)
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


int file_open(iodes_t **result, process_t *process, int dirhandle, const char *path, int flags, mode_t mode)
{
	int error = EOK;
	iodes_t *dir = NULL, *file;
	mode_t open_mode;
	size_t size;

	if ((flags & O_TRUNC) && (flags & O_RDONLY))
		return -EINVAL;

	if ((error = file_openBase(&dir, process, dirhandle, path)) < 0)
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

	if ((flags & O_APPEND) && !S_ISREG(open_mode)) {
		file_put(file);
		return -EINVAL;
	}

	/* XXX */
	file->status = flags;

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
			error = file_openDevice(file);
			if (error == EOK && file->device.port != NULL) {
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
			file->type = ftLocalSocket;
			error = -ENXIO;
			break;
		}
		case S_IFIFO: {
			file->type = ftFifo;
			file->obdes = port_obdesGet(file->fs.port, file->fs.id);
			file->pipe = NULL;

			if ((error = pipe_get(file->obdes, &file->pipe, flags)) < 0)
				break;

			while ((error = pipe_open(file->pipe)) == -EAGAIN) {
				if (file->status & O_NONBLOCK) {
					if (file->status & O_WRONLY)
						error = -ENXIO;
					else
						error = EOK;
					break;
				}
				else {
					error = file_waitForOne(file, POLLOUT, 0);
				}
			}

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


int file_resolve(iodes_t **file, process_t *process, int handle, const char *path, int flags)
{
	int err;
	if (path == NULL) {
		if ((*file = file_get(process, handle)) == NULL)
			err = -EBADF;
	}
	else {
		err = file_open(file, process, handle, path, flags, 0);
	}
	return err;
}


static int _file_dup(process_t *p, int handle, int handle2, int flags)
{
	iodes_t *f, *f2;

	if (handle == handle2)
		return -EINVAL;

	if (handle2 < 0 || (f = _file_get(p, handle)) == NULL)
		return -EBADF;

	if (flags & FD_ALLOC || handle2 >= p->hcount) {
		if ((handle2 = _fd_alloc(p, handle2)) < 0) {
			file_put(f);
			return handle2;
		}

		flags &= ~FD_ALLOC;
	}
	else if ((f2 = p->handles[handle2].file) != NULL) {
		file_put(f2);
	}

	p->handles[handle2].file = f;
	p->handles[handle2].flags = flags;
	return handle2;
}


static iodes_t *file_setRoot(iodes_t *newroot)
{
	iodes_t *oldroot;
	proc_lockSet(&file_common.lock);
	oldroot = file_common.root;
	file_common.root = newroot;
	proc_lockClear(&file_common.lock);
	return oldroot;
}


int proc_filesSetRoot(int handle, id_t id, mode_t mode)
{
	iodes_t *root, *port;
	process_t *process = proc_current()->process;

	if ((port = file_get(process, handle)) == NULL) {
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

	root->type = ftDirectory; /* TODO: check */
	root->fs.port = port->port;
	root->fs.id = id;

	file_put(file_setRoot(root));
	return EOK;
}


int proc_fileOpen(int dirhandle, const char *path, int flags, mode_t mode)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file;
	int error = EOK;

	/* FIXME - we currently use this to create directories too.. */
	if (!(mode & S_IFMT))
		mode |= S_IFREG;

	if ((error = file_open(&file, process, dirhandle, path, flags, mode)) < 0)
		return error;

	return fd_new(process, 0, 0, file);
}


/* TODO: get rid of this */
int proc_fileOid(process_t *process, int handle, oid_t *oid)
{
	int retval = -EBADF;
	iodes_t *file;

	process_lock(process);
	if ((file = _file_get(process, handle)) != NULL) {
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


int proc_fileClose(int handle)
{
	thread_t *current = proc_current();
	process_t *process = current->process;

	return fd_close(process, handle);
}


ssize_t proc_fileRead(int handle, char *buf, size_t nbyte)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file;
	ssize_t retval;
	int err;

	if ((file = file_get(process, handle)) == NULL)
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


ssize_t proc_fileWrite(int handle, const char *buf, size_t nbyte)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file;
	ssize_t retval;
	int err;
	off_t offset;

	if ((file = file_get(process, handle)) == NULL)
		return -EBADF;

	for (;;) {
		file_lock(file);
		if (file->status & O_APPEND) {
			offset = 0;
			_file_seek(file, &offset, SEEK_END); /* TODO: check return value */
		}

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


int proc_fileSeek(int handle, off_t *offset, int whence)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file;
	int retval;

	if ((file = file_get(process, handle)) == NULL)
		return -EBADF;

	file_lock(file);
	retval = _file_seek(file, offset, whence);
	file_unlock(file);
	file_put(file);
	return retval;
}


int proc_fileTruncate(int handle, off_t length)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file;
	ssize_t retval;

	if ((file = file_get(process, handle)) == NULL)
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


int proc_fileIoctl(int handle, unsigned long request, const char *indata, size_t insz, char *outdata, size_t outsz)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file;
	int retval;

	if ((file = file_get(process, handle)) == NULL)
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


int proc_fileDup(int handle, int handle2, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	int retval;

	process_lock(process);
	retval = _file_dup(process, handle, handle2, flags);
	process_unlock(process);
	return retval;
}


int proc_fileLink(int handle, const char *path, int dirhandle, const char *name, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file, *dir;
	int retval;
	const char *linkname;
	oid_t oid;

	if ((retval = file_resolve(&dir, process, dirhandle, name, O_DIRECTORY | O_PARENT)) < 0)
		return retval;

	linkname = file_basename(name);

	if ((retval = file_resolve(&file, process, handle, path, 0)) < 0) {
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


int proc_fileUnlink(int dirhandle, const char *path, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *dir;
	int retval;
	const char *linkname;

	if ((retval = file_open(&dir, process, dirhandle, path, O_DIRECTORY | O_PARENT, 0)) < 0)
		return retval;

	linkname = file_basename(path);

	file_lock(dir);
	retval = proc_objectUnlink(dir->fs.port, dir->fs.id, linkname);
	file_unlock(dir);

	file_put(dir);
	return retval;
}


static int fcntl_getFd(int handle)
{
	process_t *p = proc_current()->process;
	iodes_t *file;
	int flags;

	process_lock(p);
	if ((file = _file_get(p, handle)) == NULL) {
		process_unlock(p);
		return -EBADF;
	}

	flags = p->handles[handle].flags;
	process_unlock(p);
	file_put(file);

	return flags;
}


static int fcntl_setFd(int handle, int flags)
{
	process_t *p = proc_current()->process;
	iodes_t *file;

	process_lock(p);
	if ((file = _file_get(p, handle)) == NULL) {
		process_unlock(p);
		return -EBADF;
	}

	p->handles[handle].flags = flags;
	process_unlock(p);
	file_put(file);

	return EOK;
}


static int fcntl_getFl(int handle)
{
	process_t *p = proc_current()->process;
	iodes_t *file;
	int status;

	if ((file = file_get(p, handle)) == NULL)
		return -EBADF;

	status = file->status;
	file_put(file);

	return status;
}


static int fcntl_setFl(int handle, int val)
{
	process_t *p = proc_current()->process;
	iodes_t *file;
	int ignored = O_CREAT|O_EXCL|O_NOCTTY|O_TRUNC|O_RDONLY|O_RDWR|O_WRONLY;

	if ((file = file_get(p, handle)) == NULL)
		return -EBADF;

	file_lock(file);
	file->status = (val & ~ignored) | (file->status & ignored);
	file_unlock(file);
	file_put(file);

	return 0;
}


int proc_fileControl(int handle, int cmd, long arg)
{
	int err, flags = 0;

	switch (cmd) {
	case F_DUPFD_CLOEXEC:
		flags = FD_CLOEXEC;
		/* fallthrough */
	case F_DUPFD:
		err = proc_fileDup(handle, arg, flags | FD_ALLOC);
		break;

	case F_GETFD:
		err = fcntl_getFd(handle);
		break;

	case F_SETFD:
		err = fcntl_setFd(handle, arg);
		break;

	case F_GETFL:
		err = fcntl_getFl(handle);
		break;

	case F_SETFL:
		err = fcntl_setFl(handle, arg);
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


int proc_fileStat(int handle, const char *path, file_stat_t *buf, int flags)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	iodes_t *file;
	int err;
	port_t *port;
	id_t id;
	mode_t mode = 0;

	if (path == NULL) {
		if ((file = file_get(process, handle)) == NULL)
			return -EBADF;
	}
	else if ((err = file_openBase(&file, process, handle, path)) < 0) {
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


int proc_fileChmod(int handle, mode_t mode)
{
	return 0;
}


int proc_filesDestroy(process_t *process)
{
	int handle;

	process_lock(process);
	for (handle = 0; handle < process->hcount; ++handle)
		_fd_close(process, handle);
	vm_kfree(process->handles);
	process->handles = NULL;
	process_unlock(process);

	return EOK;
}


static int _proc_filesCopy(process_t *parent)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	int handle;

	if (process->hcount)
		return -EINVAL;

	if ((process->handles = vm_kmalloc(parent->hcount * sizeof(hades_t))) == NULL)
		return -ENOMEM;

	process->hcount = parent->hcount;
	hal_memcpy(process->handles, parent->handles, parent->hcount * sizeof(hades_t));

	for (handle = 0; handle < process->hcount; ++handle) {
		if (process->handles[handle].file != NULL)
			file_ref(process->handles[handle].file);
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
	int handle;

	process_lock(process);
	for (handle = 0; handle < process->hcount; ++handle) {
		if (process->handles[handle].file != NULL && (process->handles[handle].flags & FD_CLOEXEC))
			_fd_close(process, handle);
	}
	process_unlock(process);

	return EOK;
}


// TODO: ??
int proc_sunCreate(port_t **port, id_t *id, int dirhandle, const char *path, mode_t mode)
{
	process_t *process = proc_current()->process;
	int err;
	iodes_t *dir;
	const char *fifoname;

	/* only permission bits allowed */
	if (mode & ~(S_IRWXU | S_IRWXG | S_IRWXO))
		return -EINVAL;

	if ((err = file_resolve(&dir, process, dirhandle, path, O_PARENT | O_DIRECTORY)) < 0)
		return err;

	fifoname = file_basename(path);
	mode |= S_IFSOCK;

	if ((err = proc_objectLookup(dir->fs.port, dir->fs.id, fifoname, hal_strlen(fifoname), O_CREAT /*| O_EXCL*/, id, &mode, NULL)) == EOK) {
		*port = dir->fs.port;
	}
	file_put(dir);
	return err;
}


int proc_fifoCreate(int dirhandle, const char *path, mode_t mode)
{
	process_t *process = proc_current()->process;
	int err;
	iodes_t *dir;
	const char *fifoname;
	id_t id;

	/* only permission bits allowed */
	if (mode & ~(S_IRWXU | S_IRWXG | S_IRWXO))
		return -EINVAL;

	if ((err = file_resolve(&dir, process, dirhandle, path, O_PARENT | O_DIRECTORY)) < 0)
		return err;

	fifoname = file_basename(path);
	mode |= S_IFIFO;

	if ((err = proc_objectLookup(dir->fs.port, dir->fs.id, fifoname, hal_strlen(fifoname), O_CREAT | O_EXCL, &id, &mode, NULL)) == EOK)
		proc_objectClose(dir->fs.port, id);
	file_put(dir);
	return err;
}


int proc_deviceCreate(int dirhandle, const char *path, int portfd, id_t id, mode_t mode)
{
	process_t *process = proc_current()->process;
	int err;
	iodes_t *dir, *port = NULL;
	const char *name;
	oid_t oid;

	if ((err = file_resolve(&dir, process, dirhandle, path, O_PARENT | O_DIRECTORY)) < 0) {
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


int proc_changeDir(int handle, const char *path)
{
	process_t *process = proc_current()->process;
	iodes_t *file;
	int retval;

	if ((retval = file_resolve(&file, process, handle, path, O_DIRECTORY)) < 0)
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
	iodes_t *device, *root;
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


int proc_fsBind(int dirhandle, const char *dirpath, int fsfd, const char *fspath)
{
	process_t *process = proc_current()->process;
	iodes_t *dir, *fs, *dotdot;
	oid_t oid;
	int retval;

	if (dirpath[0] == '/' && dirpath[1] == 0) {
		dir = NULL;
		oid.port = 0;
		oid.id = 0;
	}
	else {
		if ((retval = file_resolve(&dir, process, dirhandle, dirpath, O_DIRECTORY)) < 0)
			return retval;

		if ((retval = file_resolve(&dotdot, process, dirhandle, dirpath, O_DIRECTORY | O_PARENT)) < 0) {
			file_put(dir);
			return retval;
		}

		oid.port = dotdot->fs.port->id;
		oid.id = dotdot->fs.id;
		file_put(dotdot);
	}

	if ((retval = file_resolve(&fs, process, fsfd, fspath, O_DIRECTORY)) < 0) {
		file_put(dir);
		return retval;
	}

	if (dir != NULL) {

//		FILE_LOG("binding dir=%u.%llu to fs=%u.%llu", dir->fs.port->id, dir->fs.id, fs->fs.port->id, fs->fs.id);

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

static int socket_get(process_t *process, int socket, iodes_t **file)
{
	if ((*file = file_get(process, socket)) == NULL)
		return -EBADF;

	if ((*file)->type != ftSocket && (*file)->type != ftLocalSocket) {
		file_put(*file);
		return -ENOTSOCK;
	}

	return EOK;
}


int socket_accept(process_t *process, port_t *port, id_t id, struct sockaddr *address, socklen_t *address_len, int flags)
{
	int retval;
	id_t conn_id;
	iodes_t *conn;

	retval = proc_objectAccept(port, id, &conn_id, address, address_len);

	if (retval == EOK) {
		if ((conn = file_alloc()) != NULL) {
			conn->device.port = port;
			port_ref(port);
			conn->device.id = conn_id;
			conn->type = ftSocket;
			conn->obdes = port_obdesGet(conn->device.port, conn_id);

			if ((retval = fd_new(process, 0, flags, conn)) < 0)
				file_put(conn);
		}
		else {
			proc_objectClose(port, conn_id);
			retval = -ENOMEM;
		}
	}

	return retval;
}


int proc_netAccept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags)
{
	iodes_t *file, *conn;
	process_t *process = proc_current()->process;
	int retval;
	id_t conn_id;
	int block;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	block = !(file->status & O_NONBLOCK);

	for (;;) {
		switch (file->type) {
			case ftSocket:
				retval = socket_accept(process, file->device.port, file->device.id, address, address_len, flags);
				break;
			case ftLocalSocket:
				retval = sun_accept(process, file->sun, address, address_len);
				break;
			default:
				FILE_LOG("unexpected file type");
				retval = -ENXIO;
				break;
		}

		if (!block || retval != -EAGAIN)
			break;

		if ((retval = file_waitForOne(file, POLLIN, 0)) < 0)
			break;
	}

	file_put(file);
	return retval;
}


int proc_netBind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	iodes_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	switch (file->type) {
		case ftSocket:
			retval = proc_objectBind(file->device.port, file->device.id, address, address_len);
			break;
		case ftLocalSocket:
			retval = sun_bind(process, file->sun, address, address_len);
			break;
		default:
			FILE_LOG("unexpected file type");
			retval = -ENXIO;
			break;
	}

	file_put(file);
	return retval;
}


int proc_netConnect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	iodes_t *file;
	process_t *process = proc_current()->process;
	int retval, block, inprogress = 0, err;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	block = !(file->status & O_NONBLOCK);

	for (;;) {
		switch (file->type) {
			case ftSocket:
				retval = proc_objectConnect(file->device.port, file->device.id, address, address_len);
				break;
			case ftLocalSocket:
				retval = sun_connect(process, file->sun, address, address_len);
				break;
			default:
				FILE_LOG("unexpected file type");
				retval = -ENXIO;
				break;
		}

		if (!block || retval != -EINPROGRESS)
			break;

		inprogress = 1;

		if ((retval = file_waitForOne(file, POLLOUT, 0)) < 0)
			break;
	}

	if (inprogress && retval == -EALREADY)
		retval = EOK;

	file_put(file);
	return retval;
}


int proc_netGetpeername(int socket, struct sockaddr *address, socklen_t *address_len)
{
	iodes_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	switch (file->type) {
		case ftSocket:
			if ((retval = proc_objectGetAttr(file->device.port, file->device.id, atRemoteAddr, address, *address_len)) > 0)
				*address_len = retval;
			break;
		case ftLocalSocket:
			retval = -EAFNOSUPPORT;
			break;
		default:
			FILE_LOG("unexpected file type");
			retval = -ENXIO;
			break;
	}

	file_put(file);
	return retval;
}


int proc_netGetsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
	iodes_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	switch (file->type) {
		case ftSocket:
			if ((retval = proc_objectGetAttr(file->device.port, file->device.id, atLocalAddr, address, *address_len)) > 0)
				*address_len = retval;
			break;
		case ftLocalSocket:
			retval = -EAFNOSUPPORT;
			break;
		default:
			FILE_LOG("unexpected file type");
			retval = -ENXIO;
			break;
	}

	file_put(file);
	return retval;
}


int proc_netGetsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
	iodes_t *file;
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
	iodes_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	switch (file->type) {
		case ftSocket:
			retval = proc_objectListen(file->device.port, file->device.id, backlog);
			break;
		case ftLocalSocket:
			retval = sun_listen(file->sun, backlog);
			break;
		default:
			FILE_LOG("unexpected file type");
			retval = -ENXIO;
			break;
	}

	file_put(file);
	return retval;
}


ssize_t proc_recvmsg(int socket, struct msghdr *msg, int flags)
{
	iodes_t *file;
	process_t *process = proc_current()->process;
	ssize_t retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	switch (file->type) {
		case ftSocket:
			retval = -EAFNOSUPPORT;
			break;
		case ftLocalSocket:
			retval = sun_recvmsg(file->sun, msg, flags);
			break;
		default:
			FILE_LOG("unexpected file type");
			retval = -ENXIO;
			break;
	}

	file_put(file);
	return retval;
}


ssize_t proc_sendmsg(int socket, const struct msghdr *msg, int flags)
{
	iodes_t *file;
	process_t *process = proc_current()->process;
	ssize_t retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	switch (file->type) {
		case ftSocket:
			retval = -EAFNOSUPPORT;
			break;
		case ftLocalSocket:
			retval = sun_sendmsg(file->sun, msg, flags);
			break;
		default:
			FILE_LOG("unexpected file type");
			retval = -ENXIO;
			break;
	}

	file_put(file);
	return retval;
}


int proc_netShutdown(int socket, int how)
{
	iodes_t *file;
	process_t *process = proc_current()->process;
	int retval;

	if ((retval = socket_get(process, socket, &file)) < 0)
		return retval;

	switch (file->type) {
		case ftSocket:
			retval = proc_objectShutdown(file->device.port, file->device.id, how);
			break;
		case ftLocalSocket:
			retval = -EAFNOSUPPORT;
			break;
		default:
			FILE_LOG("unexpected file type");
			retval = -ENXIO;
			break;
	}

	file_put(file);
	return retval;
}


int proc_netSetsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	iodes_t *file;
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
	iodes_t *file;
	int err;

	switch (domain) {
		case AF_INET: {
			if ((err = file_open(&file, proc_current()->process, -1, "/dev/net", 0, 0)) != EOK)
				return err;

			file->type = ftSocket;

			if (err != EOK || (err = fd_new(process, 0, 0, file)) < 0)
				file_put(file);

			break;
		}
		case AF_UNIX: {
			err = sun_socket(process, type, protocol, 0);
			break;
		}
		default:
			err = -EAFNOSUPPORT;
			break;
	}

	return err;
}


static kmsg_t *kmsg_get(process_t *p, int handle)
{
	kmsg_t *kmsg;
	if (handle < 0 || handle >= p->hcount || (kmsg = p->handles[handle].msg) == NULL || HD_TYPE(p->handles[handle].flags) != hades_msg) {
		kmsg = NULL;
	}
	return kmsg;
}


int proc_msgRespond(int porth, int handle, int error, msg_t *msg)
{
	process_t *process = proc_current()->process;
	iodes_t *portdes;
	kmsg_t *kmsg;

	if ((portdes = file_get(process, porth)) == NULL)
		return -EBADF;

	if (portdes->type != ftPort) {
		file_put(portdes);
		return -EBADF;
	}

	process_lock(process);
	if ((kmsg = kmsg_get(process, handle)) == NULL) {
		error = -EBADF;
	}
	else {
		error = port_respond(portdes->port, error, msg, kmsg);
		_fd_close(process, handle);
	}
	process_unlock(process);
	file_put(portdes);
	return error;
}


int proc_msgSend(int handle, msg_t *msg)
{
	iodes_t *portdes;
	process_t *process = proc_current()->process;
	int error;

	if ((portdes = file_get(process, handle)) == NULL)
		return -EBADF;

	if (portdes->type != ftPort) {
		file_put(portdes);
		return -EBADF;
	}

	error = port_send(portdes->port, msg);
	file_put(portdes);
	return error;
}


int proc_msgRecv(int handle, msg_t *msg)
{
	iodes_t *portdes;
	process_t *process = proc_current()->process;
	kmsg_t *kmsg;
	int error;

	if ((portdes = file_get(process, handle)) == NULL)
		return -EBADF;

	if (portdes->type != ftPort) {
		file_put(portdes);
		return -EBADF;
	}

	if ((error = port_recv(portdes->port, msg, &kmsg)) < 0) {
		file_put(portdes);
		return error;
	}

	if ((error = fd_new(process, 0, HD_MESSAGE, kmsg)) < 0) {
		port_respond(portdes->port, error, msg, kmsg);
	}

	file_put(portdes);
	return error;
}


void _file_init()
{
	file_common.root = NULL;
	proc_lockInit(&file_common.lock);
	_sun_init();
}
