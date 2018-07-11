/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../../include/posix.h"
#include "threads.h"
#include "cond.h"
#include "resource.h"
#include "name.h"
#include "posix.h"

#define MAX_FD_COUNT 32

//#define TRACE(str, ...) lib_printf("posix %x: " str "\n", proc_current()->process->id, ##__VA_ARGS__)
#define TRACE(str, ...)


enum { ftRegular, ftPipe, ftFifo, ftInetSocket, ftUnixSocket, ftTty };

enum { pxBufferedPipe, pxPipe, pxPTY };


typedef struct {
	oid_t oid;
	unsigned refs;
	off_t offset;
	unsigned status;
	lock_t lock;
	char type;
} open_file_t;


typedef struct {
	open_file_t *file;
	unsigned flags;
} fildes_t;


typedef struct {
	rbnode_t linkage;
	process_t *process;

	lock_t lock;
	int maxfd;
	fildes_t *fds;
} process_info_t;


struct {
	rbtree_t pid;
	lock_t lock;
} posix_common;


static char *strrchr(const char *s, int c)
{
	const char *p = NULL;

	do {
		if (*s == c)
			p = s;
	}
	while (*(s++));

	return (char *)p;
}


static void splitname(char *path, char **base, char **dir)
{
	char *slash;

	slash = strrchr(path, '/');

	if (slash == NULL) {
		*dir = ".";
		*base = path;
	}
	else if (slash == path) {
		*base = path + 1;
		*dir = "/";
	}
	else {
		*dir = path;
		*base = slash + 1;
		*slash = 0;
	}
}


static void posix_fileDeref(open_file_t *f)
{
	while (proc_lockSet(&f->lock) < 0);
	if (!--f->refs) {
		proc_close(f->oid, f->status);
		proc_lockDone(&f->lock);
		vm_kfree(f);
	}
	else {
		proc_lockClear(&f->lock);
	}
}


static int pinfo_cmp(rbnode_t *n1, rbnode_t *n2)
{
	process_info_t *p1 = lib_treeof(process_info_t, linkage, n1);
	process_info_t *p2 = lib_treeof(process_info_t, linkage, n2);

	if (p1->process->id < p2->process->id)
		return -1;

	else if (p1->process->id > p2->process->id)
		return 1;

	return 0;
}


process_info_t *pinfo_find(unsigned int pid)
{
	process_t p;
	process_info_t pi, *r;

	p.id = pid;
	pi.process = &p;

	proc_lockSet(&posix_common.lock);
	r = lib_treeof(process_info_t, linkage, lib_rbFind(&posix_common.pid, &pi.linkage));
	proc_lockClear(&posix_common.lock);

	return r;
}


int posix_clone(int ppid)
{
	TRACE("clone(%x)", ppid);

	process_info_t *p, *pp;
	process_t *proc;
	int i;
	oid_t console = {0, 0};
	open_file_t *f;

	proc = proc_current()->process;

	if ((p = vm_kmalloc(sizeof(process_info_t))) == NULL)
		return -ENOMEM;

	proc_lockInit(&p->lock);

	if ((pp = pinfo_find(ppid)) != NULL) {
		TRACE("clone: got parent");
		proc_lockSet(&pp->lock);
		p->maxfd = pp->maxfd;
	}
	else {
		p->maxfd = MAX_FD_COUNT - 1;
	}

	p->process = proc;

	if ((p->fds = vm_kmalloc((p->maxfd + 1) * sizeof(fildes_t))) == NULL) {
		vm_kfree(p);
		return -ENOMEM;
	}

	if (pp != NULL) {
		hal_memcpy(p->fds, pp->fds, (pp->maxfd + 1) * sizeof(fildes_t));

		for (i = 0; i <= p->maxfd; ++i) {
			if ((f = p->fds[i].file) != NULL) {
				proc_lockSet(&f->lock);
				++f->refs;
				proc_lockClear(&f->lock);
			}
		}

		proc_lockClear(&pp->lock);
	}
	else {
		hal_memset(p->fds, 0, (p->maxfd + 1) * sizeof(fildes_t));

		for (i = 0; i < 3; ++i) {
			if ((f = p->fds[i].file = vm_kmalloc(sizeof(open_file_t))) == NULL)
				return -ENOMEM;

			proc_lockInit(&f->lock);
			f->refs = 1;
			f->offset = 0;
			f->type = ftTty;
			p->fds[i].flags = 0;
			hal_memcpy(&f->oid, &console, sizeof(oid_t));
		}

		p->fds[0].file->status = O_RDONLY;
		p->fds[1].file->status = O_WRONLY;
		p->fds[2].file->status = O_WRONLY;
	}

	proc_lockSet(&posix_common.lock);
	lib_rbInsert(&posix_common.pid, &p->linkage);
	proc_lockClear(&posix_common.lock);

	return EOK;
}


int posix_fork()
{
	int pid;

	if (!(pid = proc_vfork())) {
		proc_copyexec();
		/* Not reached */
	}

	return pid;
}


int posix_exec(void)
{
	TRACE("exec");

	process_info_t *p;
	open_file_t *f;
	int fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	for (fd = 0; fd <= p->maxfd; ++fd) {
		if ((f = p->fds[fd].file) != NULL && p->fds[fd].flags & O_CLOEXEC) {
			posix_fileDeref(f);
			p->fds[fd].file = NULL;
		}
	}
	proc_lockClear(&p->lock);

	return 0;
}


int posix_exit(process_t *process)
{
	TRACE("exit(%x)", process->id);

	process_info_t *p;
	open_file_t *f;
	int fd;

	if ((p = pinfo_find(process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	for (fd = 0; fd <= p->maxfd; ++fd) {
		if ((f = p->fds[fd].file) != NULL)
			posix_fileDeref(f);
	}

	proc_lockSet(&posix_common.lock);
	lib_rbRemove(&posix_common.pid, &p->linkage);
	proc_lockClear(&posix_common.lock);

	vm_kfree(p->fds);
	proc_lockDone(&p->lock);
	vm_kfree(p);

	return 0;

}


static int posix_create(const char *filename, int type, mode_t mode, oid_t dev, oid_t *oid)
{
	TRACE("posix_create(%s, %d)", filename, mode);

	int err;
	oid_t dir;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(filename);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, filename, namelen + 1);

	splitname(name, &basename, &dirname);

	do {
		if ((err = proc_lookup(dirname, &dir)) < 0)
			break;

		if ((err = proc_create(dir.port, type, mode, dev, dir, basename, oid)) < 0)
			break;

		return EOK;
	} while (0);

	vm_kfree(name);
	return err;
}


/* TODO: handle O_CREAT and O_EXCL */
int posix_open(const char *filename, int oflag, char *ustack)
{
	TRACE("open(%s, %d, %d)", filename, oflag, mode);
	oid_t oid, dev = {0, 0}, pipesrv;
	int fd = 0, err;
	process_info_t *p;
	open_file_t *f;
	mode_t mode;

	if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
		return -1;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		while (p->fds[fd].file != NULL && fd++ < p->maxfd);

		if (fd > p->maxfd)
			break;

		if ((f = p->fds[fd].file = vm_kmalloc(sizeof(open_file_t))) == NULL)
			break;

		proc_lockInit(&f->lock);

		do {
			if (proc_lookup(filename, &oid) == EOK) {
				/* pass */
			}
			else if (oflag & O_CREAT) {
				GETFROMSTACK(ustack, mode_t, mode, 2);

				if (posix_create(filename, 1 /* otFile */, mode, dev, &oid) < 0)
					break;
			}
			else {
				break;
			}

			if ((err = proc_open(oid, oflag)) < 0)
				break;

			/* TODO: truncate, append */

			p->fds[fd].flags = oflag & O_CLOEXEC;

			if (!err) {
				hal_memcpy(&f->oid, &oid, sizeof(oid));
			}
			else {
				f->oid.port = oid.port;
				f->oid.id = err;
			}

			f->refs = 1;
			f->offset = 0;
			/* TODO: check for other types */
			f->type = oid.port == pipesrv.port ? ftPipe : ftRegular;
			f->status = oflag & ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_CLOEXEC);

			proc_lockClear(&p->lock);
			return fd;
		} while (0);

		p->fds[fd].file = NULL;
		proc_lockDone(&f->lock);
		vm_kfree(f);

	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_close(int fildes)
{
	TRACE("close(%d)", fildes);
	open_file_t *f;
	process_info_t *p;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		posix_fileDeref(f);
		p->fds[fildes].file = NULL;
		proc_lockClear(&p->lock);
		return 0;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_read(int fildes, void *buf, size_t nbyte)
{
	TRACE("read(%d, %p, %u)", fildes, buf, nbyte);

	open_file_t *f;
	process_info_t *p;
	int rcnt;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		if ((rcnt = proc_read(f->oid, f->offset, buf, nbyte, f->status)) < 0)
			break;

		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);

		proc_lockClear(&p->lock);
		return rcnt;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_write(int fildes, void *buf, size_t nbyte)
{
	TRACE("write(%d, %p, %u)", fildes, buf, nbyte);

	open_file_t *f;
	process_info_t *p;
	int rcnt;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		if ((rcnt = proc_write(f->oid, f->offset, buf, nbyte, f->status)) < 0)
			break;

		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);

		proc_lockClear(&p->lock);
		return rcnt;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_dup(int fildes)
{
	TRACE("dup(%d)", fildes);

	process_info_t *p;
	int newfd = 0;
	open_file_t *f;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		while (p->fds[newfd].file != NULL && newfd++ < p->maxfd);

		if (newfd > p->maxfd)
			break;

		p->fds[newfd].file = f;
		p->fds[newfd].flags = 0;

		proc_lockSet(&f->lock);
		f->refs++;
		proc_lockClear(&f->lock);

		proc_lockClear(&p->lock);
		return newfd;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


/* FIXME: handle fildes == fildes2 */
int _posix_dup2(process_info_t *p, int fildes, int fildes2)
{
	open_file_t *f, *f2;

	if (fildes < 0 || fildes > p->maxfd)
		return -1;

	if (fildes2 < 0 || fildes2 > p->maxfd)
		return -1;

	if ((f = p->fds[fildes].file) == NULL)
		return -1;

	if ((f2 = p->fds[fildes2].file) != NULL) {
		p->fds[fildes2].file = NULL;
		posix_fileDeref(f2);
	}

	p->fds[fildes2].file = f;
	p->fds[fildes2].flags = 0;

	proc_lockSet(&f->lock);
	f->refs++;
	proc_lockClear(&f->lock);

	return fildes2;
}


int posix_dup2(int fildes, int fildes2)
{
	TRACE("dup2(%d, %d)", fildes, fildes2);

	process_info_t *p;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	fildes2 = _posix_dup2(p, fildes, fildes2);
	proc_lockClear(&p->lock);

	return fildes2;
}


int posix_pipe(int fildes[2])
{
	TRACE("pipe(%p)", fildes);

	process_info_t *p;
	open_file_t *fi, *fo;
	oid_t oid = {0};
	oid_t pipesrv;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
			break;

		if (proc_create(pipesrv.port, pxBufferedPipe, O_RDONLY | O_WRONLY, oid, pipesrv, NULL, &oid) < 0)
			break;

		fildes[0] = 0;
		while (p->fds[fildes[0]].file != NULL && fildes[0]++ < p->maxfd);

		fildes[1] = fildes[0] + 1;
		while (p->fds[fildes[1]].file != NULL && fildes[1]++ < p->maxfd);

		if (fildes[0] > p->maxfd || fildes[1] > p->maxfd)
			break;

		p->fds[fildes[0]].flags = p->fds[fildes[1]].flags = 0;

		if ((fo = p->fds[fildes[0]].file = vm_kmalloc(sizeof(open_file_t))) == NULL) {
			/* FIXME: destroy pipe */
			break;
		}

		proc_lockInit(&fo->lock);
		hal_memcpy(&fo->oid, &oid, sizeof(oid));
		fo->refs = 1;
		fo->offset = 0;
		fo->type = ftPipe;
		fo->status = O_RDONLY;

		if ((fi = p->fds[fildes[1]].file = vm_kmalloc(sizeof(open_file_t))) == NULL) {
			p->fds[fildes[0]].file = NULL;
			proc_lockDone(&fo->lock);
			vm_kfree(fo);
			/* FIXME: destroy pipe */
			break;
		}

		proc_lockInit(&fi->lock);
		hal_memcpy(&fi->oid, &oid, sizeof(oid));
		fi->refs = 1;
		fi->offset = 0;
		fi->type = ftPipe;
		fi->status = O_WRONLY;

		proc_lockClear(&p->lock);
		return 0;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_mkfifo(const char *pathname, mode_t mode)
{
	TRACE("mkfifo(%s, %x)", pathname, mode);

	process_info_t *p;
	oid_t oid = {0, 0}, file;
	oid_t pipesrv;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
			break;

		if (proc_create(pipesrv.port, pxPipe, 0, oid, pipesrv, NULL, &oid) < 0)
			break;

		/* link pipe in posix server */
		if (proc_link(oid, oid, pathname) < 0)
			break;

		/* create pipe in filesystem */
		if (posix_create(pathname, 2 /* otDev */, mode, oid, &file) < 0)
			break;

		proc_lockClear(&p->lock);
		return 0;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_link(const char *path1, const char *path2)
{
	process_info_t *p;
	oid_t oid, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(path2);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, path2, namelen + 1);

	splitname(name, &basename, &dirname);

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if ((err = proc_lookup(dirname, &dir)) < 0)
			break;

		if ((err = proc_lookup(path1, &oid)) < 0)
			break;

		if ((err = proc_link(dir, oid, basename)) < 0)
			break;

		if (dir.port != oid.port) {
			/* Signal link to device */
			/* FIXME: refcount here? */
			if ((err = proc_link(oid, oid, path2)) < 0)
				break;
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	proc_lockClear(&p->lock);
	return err;
}


int posix_unlink(const char *pathname)
{
	process_info_t *p;
	oid_t oid, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(pathname);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, pathname, namelen + 1);

	splitname(name, &basename, &dirname);

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if ((err = proc_lookup(dirname, &dir)) < 0)
			break;

		if ((err = proc_lookup(pathname, &oid)) < 0)
			break;

		if ((err = proc_unlink(dir, oid, basename)) < 0)
			break;

		if (dir.port != oid.port) {
			/* Signal unlink to device */
			/* FIXME: refcount here? */
			if ((err = proc_unlink(oid, oid, pathname)) < 0)
				break;
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	proc_lockClear(&p->lock);
	return err;
}


off_t posix_lseek(int fildes, off_t offset, int whence)
{
	return -1;
}


int posix_ftruncate(int fildes, off_t length)
{
	return -1;
}


int posix_fcntl(unsigned int fd, unsigned int cmd, char *ustack)
{
	TRACE("fcntl(%d, %u, %u)", fd, cmd, arg);

	int err = -1;
	process_info_t *p;
	unsigned long arg;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	switch (cmd) {
	case F_DUPFD:
		GETFROMSTACK(ustack, unsigned long, arg, 2);
		while (p->fds[arg].file != NULL && arg++ < p->maxfd);
		err = _posix_dup2(p, fd, arg);
		break;

	case F_GETFD:
		break;

	case F_SETFD:
		GETFROMSTACK(ustack, unsigned long, arg, 2);

		switch (arg) {
		case FD_CLOEXEC:
			err = 0;
			break;
		default:
			break;
		}
		break;

	case F_GETFL:
	case F_SETFL:
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
	case F_GETOWN:
	case F_SETOWN:
	default:
		break;
	}

	proc_lockClear(&p->lock);
	return err;
}


void posix_init(void)
{
	proc_lockInit(&posix_common.lock);
	lib_rbInit(&posix_common.pid, pinfo_cmp, NULL);
}
