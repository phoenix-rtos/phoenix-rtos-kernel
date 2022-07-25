/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski, Michal Miroslaw
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../hal/hal.h"
#include "../include/errno.h"
#include "../include/ioctl.h"
#include "../include/limits.h"
#include "../proc/proc.h"

#include "posix.h"
#include "posix_private.h"
#include "../lib/cbuffer.h"

#ifdef CPU_STM32
#define MAX_FD_COUNT 8
#else
#define MAX_FD_COUNT 512
#endif

//#define TRACE(str, ...) lib_printf("posix %x: " str "\n", proc_current()->process->id, ##__VA_ARGS__)
#define TRACE(str, ...)

#define POLL_INTERVAL 100000


enum { atMode = 0, atUid, atGid, atSize, atBlocks, atIOBlock, atType, atPort, atPollStatus, atEventMask, atCTime, atMTime, atATime, atLinks, atDev };


/* TODO: copied from libphoenix/posixsrv/posixsrv.h */
enum { evAdd = 0x1, evDelete = 0x2, evEnable = 0x4, evDisable = 0x8, evOneshot = 0x10, evClear = 0x20, evDispatch = 0x40 };

typedef struct {
	oid_t oid;
	unsigned flags;
	unsigned short types;
} evsub_t;


typedef struct _event_t {
	oid_t oid;
	unsigned type;

	unsigned flags;
	unsigned count;
	unsigned data;
} event_t;



struct {
	rbtree_t pid;
	lock_t lock;
	id_t fresh;
	char hostname[HOST_NAME_MAX + 1];
} posix_common;


process_info_t *_pinfo_find(unsigned int pid)
{
	process_info_t pi, *r;
	pi.process = pid;

	if ((r = lib_treeof(process_info_t, linkage, lib_rbFind(&posix_common.pid, &pi.linkage))) != NULL)
		r->refs++;

	return r;
}


process_info_t *pinfo_find(unsigned int pid)
{
	process_info_t *r;
	proc_lockSet(&posix_common.lock);
	r = _pinfo_find(pid);
	proc_lockClear(&posix_common.lock);
	return r;
}


void posix_destroy(process_info_t *p)
{
	// lib_printf("removing %d\n", p->process);
	proc_lockSet(&posix_common.lock);
	lib_rbRemove(&posix_common.pid, &p->linkage);
	proc_lockClear(&posix_common.lock);

	vm_kfree(p->fds);
	proc_lockDone(&p->lock);
	vm_kfree(p);
}


void pinfo_put(process_info_t *p)
{
	int remaining;

	proc_lockSet(&posix_common.lock);
	remaining = --p->refs;
	proc_lockClear(&posix_common.lock);

	if (!remaining)
		posix_destroy(p);
}


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


void splitname(char *path, char **base, char **dir)
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


int posix_fileDeref(open_file_t *f)
{
	int err = EOK;

	proc_lockSet(&f->lock);
	if (!--f->refs) {
		if (f->type == ftUnixSocket) {
			err = unix_close(f->oid.id);
		}
		else {
			while ((err = proc_close(f->oid, f->status)) == -EINTR) ;
		}

		proc_lockDone(&f->lock);
		vm_kfree(f);
	}
	else {
		proc_lockClear(&f->lock);
	}
	return err;
}


static void posix_putUnusedFile(process_info_t *p, int fd)
{
	open_file_t *f;

	f = p->fds[fd].file;
	proc_lockDone(&f->lock);
	vm_kfree(f);
	p->fds[fd].file = NULL;
}


int posix_getOpenFile(int fd, open_file_t **f)
{
	process_info_t *p;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	proc_lockSet(&p->lock);
	if (fd < 0 || fd > p->maxfd || (*f = p->fds[fd].file) == NULL) {
		proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	proc_lockSet(&(*f)->lock);
	(*f)->refs++;
	proc_lockClear(&(*f)->lock);
	proc_lockClear(&p->lock);

	pinfo_put(p);
	return 0;
}


int posix_newFile(process_info_t *p, int fd)
{
	open_file_t *f;

	proc_lockSet(&p->lock);

	while (p->fds[fd].file != NULL && fd++ < p->maxfd);

	if (fd > p->maxfd) {
		proc_lockClear(&p->lock);
		return -ENFILE;
	}

	if ((f = p->fds[fd].file = vm_kmalloc(sizeof(open_file_t))) == NULL) {
		proc_lockClear(&p->lock);
		return -ENOMEM;
	}

	proc_lockClear(&p->lock);

	hal_memset(f, 0, sizeof(open_file_t));
	proc_lockInit(&f->lock);
	f->refs = 1;
	f->offset = 0;

	return fd;
}


int _posix_addOpenFile(process_info_t *p, open_file_t *f, unsigned int flags)
{
	int fd = 0;

	while (p->fds[fd].file != NULL && fd++ < p->maxfd)
		;

	if (fd > p->maxfd)
		return -ENFILE;

	p->fds[fd].file = f;
	p->fds[fd].flags = flags;

	return fd;
}


static int pinfo_cmp(rbnode_t *n1, rbnode_t *n2)
{
	process_info_t *p1 = lib_treeof(process_info_t, linkage, n1);
	process_info_t *p2 = lib_treeof(process_info_t, linkage, n2);

	if (p1->process < p2->process)
		return -1;

	else if (p1->process > p2->process)
		return 1;

	return 0;
}


int posix_truncate(oid_t *oid, off_t length)
{
	msg_t msg;
	int err = -EINVAL;

	if (oid->port != US_PORT && length >= 0) {
		hal_memset(&msg, 0, sizeof(msg));
		msg.type = mtTruncate;
		hal_memcpy(&msg.i.io.oid, oid, sizeof(oid_t));
		msg.i.io.len = length;
		err = proc_send(oid->port, &msg);
	}

	return err;
}


int posix_clone(int ppid)
{
	TRACE("clone(%x)", ppid);

	process_info_t *p, *pp;
	process_t *proc;
	int i;
	oid_t console;
	open_file_t *f;

	proc = proc_current()->process;

	if ((p = vm_kmalloc(sizeof(process_info_t))) == NULL)
		return -ENOMEM;

	hal_memset(&console, 0, sizeof(oid_t));
	proc_lockInit(&p->lock);
	p->children = NULL;
	p->zombies = NULL;
	p->wait = NULL;
	p->next = p->prev = NULL;
	p->refs = 1;

	if ((pp = pinfo_find(ppid)) != NULL) {
		TRACE("clone: got parent");
		proc_lockSet(&pp->lock);
		p->maxfd = pp->maxfd;
		LIST_ADD(&pp->children, p);
		p->parent = ppid;
	}
	else {
		p->parent = 0;
		p->maxfd = MAX_FD_COUNT - 1;
	}

	p->process = proc->id;

	if ((p->fds = vm_kmalloc((p->maxfd + 1) * sizeof(fildes_t))) == NULL) {
		vm_kfree(p);
		if (pp != NULL) {
			proc_lockClear(&pp->lock);
			pinfo_put(pp);
		}
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

	if (pp != NULL) {
		p->pgid = ppid;
		pinfo_put(pp);
	}
	else {
		p->pgid = p->process;
	}

	proc_lockSet(&posix_common.lock);
	lib_rbInsert(&posix_common.pid, &p->linkage);
	proc_lockClear(&posix_common.lock);

	return EOK;
}


int posix_fork()
{
	TRACE("fork()");

	int pid;

	return -ENOSYS;

	// if (!(pid = proc_vfork())) {
	// 	proc_copyexec();
	// 	/* Not reached */
	// }

	return pid;
}


int posix_exec(void)
{
	TRACE("exec()");

	process_info_t *p;
	open_file_t *f;
	int fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	for (fd = 0; fd <= p->maxfd; ++fd) {
		if ((f = p->fds[fd].file) != NULL && p->fds[fd].flags & FD_CLOEXEC) {
			posix_fileDeref(f);
			p->fds[fd].file = NULL;
		}
	}
	proc_lockClear(&p->lock);

	pinfo_put(p);
	return 0;
}


int posix_exit(process_info_t *p, int code)
{
	open_file_t *f;
	int fd;

	p->exitcode = code;

	proc_lockSet(&p->lock);
	for (fd = 0; fd <= p->maxfd; ++fd) {
		if ((f = p->fds[fd].file) != NULL)
			posix_fileDeref(f);
	}
	proc_lockClear(&p->lock);

	return 0;
}


static int posix_create(const char *filename, int type, mode_t mode, oid_t dev, oid_t *oid)
{
	TRACE("posix_create(%s, %d)", filename, mode);

	int err;
	oid_t dir;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(filename) + 1;
	if ((name = vm_kmalloc(namelen)) == NULL)
		return -ENOMEM;
	hal_memcpy(name, filename, namelen);

	splitname(name, &basename, &dirname);

	do {
		if ((err = proc_lookup(dirname, NULL, &dir)) < 0)
			break;

		if ((err = proc_create(dir.port, type, mode, dev, dir, basename, oid)) < 0)
			break;

		err = EOK;
	} while (0);

	vm_kfree(name);
	return err;
}


/* TODO: handle O_CREAT and O_EXCL */
int posix_open(const char *filename, int oflag, char *ustack)
{
	TRACE("open(%s, %d, %d)", filename, oflag);
	oid_t ln, oid, dev, pipesrv;
	int fd = 0, err = 0;
	process_info_t *p;
	open_file_t *f;
	mode_t mode;

	hal_memset(&pipesrv, 0xff, sizeof(oid_t));

	if ((proc_lookup("/dev/posix/pipes", NULL, &pipesrv)) < 0)
		; /* that's fine */

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&dev, 0, sizeof(oid_t));

	proc_lockSet(&p->lock);

	do {
		while (p->fds[fd].file != NULL && fd++ < p->maxfd);

		if (fd > p->maxfd || (f = p->fds[fd].file = vm_kmalloc(sizeof(open_file_t))) == NULL) {
			err = -EBADF;
			break;
		}

		proc_lockInit(&f->lock);
		proc_lockClear(&p->lock);

		do {
			if ((err = proc_lookup(filename, &ln, &oid)) == EOK) {
				/* pass */
			}
			else if (err == -ENOENT && oflag & O_CREAT) {
				GETFROMSTACK(ustack, mode_t, mode, 2);

				if (posix_create(filename, 1 /* otFile */, mode | S_IFREG, dev, &oid) < 0) {
					err = -EIO;
					break;
				}
				hal_memcpy(&ln, &oid, sizeof(oid_t));
			}
			else {
				break;
			}

			if (oid.port != US_PORT && (err = proc_open(oid, oflag)) < 0)
				break;

			proc_lockSet(&p->lock);
			p->fds[fd].flags = oflag & O_CLOEXEC ? FD_CLOEXEC : 0;
			proc_lockClear(&p->lock);

			if (!err) {
				hal_memcpy(&f->oid, &oid, sizeof(oid));
			}
			else {
				/* multiplexer, e.g. /dev/ptmx */
				f->oid.port = oid.port;
				f->oid.id = err;
			}

			hal_memcpy(&f->ln, &ln, sizeof(ln));

			f->refs = 1;

			/* TODO: check for other types */
			if (oid.port == US_PORT)
				f->type = ftUnixSocket;
			else if (oid.port == pipesrv.port)
				f->type = ftPipe;
			else
				f->type = ftRegular;

			if (oflag & O_APPEND)
				f->offset = proc_size(f->oid);
			else
				f->offset = 0;

			if (oflag & O_TRUNC)
				posix_truncate(&f->oid, 0);

			f->status = oflag & ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_CLOEXEC);

			pinfo_put(p);
			return fd;
		} while (0);

		proc_lockSet(&p->lock);
		p->fds[fd].file = NULL;
		proc_lockDone(&f->lock);
		vm_kfree(f);

	} while (0);

	proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


int posix_close(int fildes)
{
	TRACE("close(%d)", fildes);
	open_file_t *f;
	process_info_t *p;
	int err = -EBADF;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		p->fds[fildes].file = NULL;
		proc_lockClear(&p->lock);

		pinfo_put(p);
		return posix_fileDeref(f);
	} while (0);

	proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


ssize_t posix_read(int fildes, void *buf, size_t nbyte)
{
	TRACE("read(%d, %p, %u)", fildes, buf, nbyte);

	open_file_t *f;
	ssize_t rcnt;
	off_t offs;
	unsigned int status;
	int err;

	if ((err = posix_getOpenFile(fildes, &f)) < 0)
		return err;

	if (f->status & O_WRONLY) {
		posix_fileDeref(f);
		return -EBADF;
	}

	proc_lockSet(&f->lock);
	offs = f->offset;
	status = f->status;
	proc_lockClear(&f->lock);

	if (f->type == ftUnixSocket)
		rcnt = unix_recvfrom(f->oid.id, buf, nbyte, 0, NULL, 0);
	else
		rcnt = proc_read(f->oid, offs, buf, nbyte, status);

	if (rcnt > 0) {
		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);
	}

	posix_fileDeref(f);

	return rcnt;
}


ssize_t posix_write(int fildes, void *buf, size_t nbyte)
{
	TRACE("write(%d, %p, %u)", fildes, buf, nbyte);

	open_file_t *f;
	ssize_t rcnt;
	off_t offs;
	unsigned int status;
	int err;

	if ((err = posix_getOpenFile(fildes, &f)) < 0)
		return err;

	if (f->status & O_RDONLY) {
		posix_fileDeref(f);
		return -EBADF;
	}

	proc_lockSet(&f->lock);
	offs = f->offset;
	status = f->status;
	proc_lockClear(&f->lock);

	if (f->type == ftUnixSocket)
		rcnt = unix_sendto(f->oid.id, buf, nbyte, 0, NULL, 0);
	else
		rcnt = proc_write(f->oid, offs, buf, nbyte, status);

	if (rcnt > 0) {
		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);
	}

	posix_fileDeref(f);

	return rcnt;
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
		pinfo_put(p);

		return newfd;
	} while (0);

	proc_lockClear(&p->lock);
	pinfo_put(p);
	return -EBADF;
}


/* FIXME: handle fildes == fildes2 */
int _posix_dup2(process_info_t *p, int fildes, int fildes2)
{
	open_file_t *f, *f2;

	if (fildes < 0 || fildes > p->maxfd)
		return -EBADF;

	if (fildes2 < 0 || fildes2 > p->maxfd)
		return -EBADF;

	if ((f = p->fds[fildes].file) == NULL)
		return -EBADF;

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
	pinfo_put(p);

	return fildes2;
}


int posix_pipe(int fildes[2])
{
	TRACE("pipe(%p)", fildes);

	process_info_t *p;
	open_file_t *fi, *fo;
	oid_t oid;
	oid_t pipesrv;
	int res;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&oid, 0, sizeof(oid));

	if ((res = proc_lookup("/dev/posix/pipes", NULL, &pipesrv)) < 0) {
		pinfo_put(p);
		return res == -EINTR ? res : -ENOSYS;
	}

	if ((res = proc_create(pipesrv.port, pxBufferedPipe, O_RDONLY | O_WRONLY, oid, pipesrv, NULL, &oid)) < 0) {
		pinfo_put(p);
		return res;
	}

	if ((fo = vm_kmalloc(sizeof(open_file_t))) == NULL) {
		pinfo_put(p);
		/* FIXME: destroy pipe */
		return -ENOMEM;
	}

	if ((fi = vm_kmalloc(sizeof(open_file_t))) == NULL) {
		vm_kfree(fo);
		pinfo_put(p);
		/* FIXME: destroy pipe */
		return -ENOMEM;
	}

	fildes[0] = 0;

	proc_lockSet(&p->lock);
	while (p->fds[fildes[0]].file != NULL && fildes[0]++ < p->maxfd);

	fildes[1] = fildes[0] + 1;
	while (p->fds[fildes[1]].file != NULL && fildes[1]++ < p->maxfd);

	if (fildes[0] > p->maxfd || fildes[1] > p->maxfd) {
		proc_lockClear(&p->lock);

		vm_kfree(fo);
		vm_kfree(fi);

		pinfo_put(p);
		return -EMFILE;
	}

	p->fds[fildes[0]].flags = p->fds[fildes[1]].flags = 0;

	p->fds[fildes[0]].file = fo;
	proc_lockInit(&fo->lock);
	hal_memcpy(&fo->oid, &oid, sizeof(oid));
	fo->refs = 1;
	fo->offset = 0;
	fo->type = ftPipe;
	fo->status = O_RDONLY;

	p->fds[fildes[1]].file = fi;
	proc_lockInit(&fi->lock);
	hal_memcpy(&fi->oid, &oid, sizeof(oid));
	fi->refs = 1;
	fi->offset = 0;
	fi->type = ftPipe;
	fi->status = O_WRONLY;

	proc_lockClear(&p->lock);
	pinfo_put(p);
	return 0;
}


int posix_mkfifo(const char *pathname, mode_t mode)
{
	TRACE("mkfifo(%s, %x)", pathname, mode);

	oid_t oid, file;
	oid_t pipesrv;

	hal_memset(&oid, 0, sizeof(oid));

	if ((proc_lookup("/dev/posix/pipes", NULL, &pipesrv)) < 0)
		return -ENOSYS;

	if (proc_create(pipesrv.port, pxBufferedPipe, 0, oid, pipesrv, NULL, &oid) < 0)
		return -EIO;

	/* link pipe in posix server */
	if (proc_link(oid, oid, pathname) < 0)
		return -EIO;

	/* create pipe in filesystem */
	if (posix_create(pathname, 2 /* otDev */, mode | S_IFIFO, oid, &file) < 0)
		return -EIO;

	return 0;
}


int posix_chmod(const char *pathname, mode_t mode)
{
	TRACE("chmod(%s, %x)", pathname, mode);

	oid_t oid;
	msg_t msg;
	int err;

	if (proc_lookup(pathname, &oid, NULL) < 0)
		return -ENOENT;

	hal_memset(&msg, 0, sizeof(msg));
	hal_memcpy(&msg.i.attr.oid, &oid, sizeof(oid));

	msg.type = mtSetAttr;
	msg.i.attr.type = atMode;
	msg.i.attr.val = mode & ALLPERMS;

	if (((err = proc_send(oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
		return err;

	return EOK;
}


int posix_link(const char *path1, const char *path2)
{
	TRACE("link(%s, %s)", path1, path2);

	oid_t oid, dev, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(path2) + 1;
	if ((name = vm_kmalloc(namelen)) == NULL)
		return -ENOMEM;
	hal_memcpy(name, path2, namelen);

	splitname(name, &basename, &dirname);

	do {
		if ((err = proc_lookup(dirname, NULL, &dir)) < 0)
			break;

		if ((err = proc_lookup(path1, &oid, &dev)) < 0)
			break;

		if (oid.port != dir.port) {
			err = -EXDEV;
			break;
		}

		if ((err = proc_link(dir, oid, basename)) < 0)
			break;

		if (dev.port != oid.port) {
			/* Signal link to device */
			/* FIXME: refcount here? */
			if ((err = proc_link(dev, dev, path2)) < 0)
				break;
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	return err;
}


int posix_unlink(const char *pathname)
{
	TRACE("unlink(%s)", pathname);

	oid_t oid, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(pathname) + 1;
	if ((name = vm_kmalloc(namelen)) == NULL)
		return -ENOMEM;
	hal_memcpy(name, pathname, namelen);

	splitname(name, &basename, &dirname);

	do {
		if ((err = proc_lookup(dirname, NULL, &dir)) < 0)
			break;

		if ((err = proc_lookup(pathname, NULL, &oid)) < 0)
			break;

		if ((err = proc_unlink(dir, oid, basename)) < 0)
			break;

		if (dir.port != oid.port) {
			if (oid.port == US_PORT)
				unix_unlink(oid.id);

			/* Signal unlink to device */
			/* FIXME: refcount here? */
			else if ((err = proc_unlink(oid, oid, pathname)) < 0)
				break;
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	return err;
}


off_t posix_lseek(int fildes, off_t offset, int whence)
{
	TRACE("seek(%d, %d, %d)", fildes, offset, whence);

	open_file_t *f;
	off_t scnt;
	int err;

	if ((err = posix_getOpenFile(fildes, &f)))
		return err;

	/* TODO: Find a better way to check fd type */
	if ((scnt = proc_size(f->oid)) < 0)
		return -ESPIPE;

	proc_lockSet(&f->lock);
	switch (whence) {
	case SEEK_SET:
		f->offset = offset;
		scnt = f->offset;
		break;

	case SEEK_CUR:
		f->offset += offset;
		scnt = f->offset;
		break;

	case SEEK_END:
		scnt += offset;
		f->offset = scnt;
		break;

	default:
		scnt = -EINVAL;
		break;
	}
	proc_lockClear(&f->lock);

	posix_fileDeref(f);

	return scnt;
}


int posix_ftruncate(int fildes, off_t length)
{
	TRACE("ftruncate(%d)", fildes);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(fildes, &f)))
		err = posix_truncate(&f->oid, length);

	posix_fileDeref(f);
	return err;
}


int posix_fstat(int fd, struct stat *buf)
{
	TRACE("fstat(%d)", fd);

	open_file_t *f;
	msg_t msg;
	int err;

	if ((err = posix_getOpenFile(fd, &f)) < 0)
		return err;

	hal_memset(buf, 0, sizeof(struct stat));
	hal_memset(&msg, 0, sizeof(msg_t));

	buf->st_dev = f->ln.port;
	buf->st_ino = (int)f->ln.id; /* FIXME */
	buf->st_rdev = f->oid.port;

	if (f->type == ftRegular) {
		msg.type = mtGetAttr;
		hal_memcpy(&msg.i.attr.oid, &f->oid, sizeof(oid_t));

		do {
			msg.i.attr.type = atMTime;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_mtime = msg.o.attr.val;

			msg.i.attr.type = atATime;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_atime = msg.o.attr.val;

			msg.i.attr.type = atCTime;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_ctime = msg.o.attr.val;

			msg.i.attr.type = atLinks;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_nlink = msg.o.attr.val;

			msg.i.attr.type = atMode;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_mode = msg.o.attr.val;

			msg.i.attr.type = atUid;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_uid = msg.o.attr.val;

			msg.i.attr.type = atGid;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_gid = msg.o.attr.val;

			msg.i.attr.type = atSize;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_size = msg.o.attr.val;

			msg.i.attr.type = atBlocks;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_blocks = msg.o.attr.val;

			msg.i.attr.type = atIOBlock;
			if (((err = proc_send(f->oid.port, &msg)) < 0) || ((err = msg.o.attr.err) < 0))
				break;
			buf->st_blksize = msg.o.attr.val;
		} while (0);
	}
	else {
		switch (f->type) {
			case ftRegular:
				break;
			case ftPipe:
			case ftFifo:
				buf->st_mode = S_IFIFO;
				break;
			case ftInetSocket:
			case ftUnixSocket:
				buf->st_mode = S_IFSOCK;
				break;
			case ftTty:
				buf->st_mode = S_IFCHR;
				break;
			default:
				buf->st_mode = 0;
				break;
		}

		buf->st_uid = 0;
		buf->st_gid = 0;
		buf->st_size = proc_size(f->oid);
	}

	posix_fileDeref(f);

	return err;
}


int posix_fsync(int fd)
{
	TRACE("fsync(%d)", fd);

	open_file_t *f;
	msg_t msg;
	int err;

	if ((err = posix_getOpenFile(fd, &f)) < 0)
		return err;

	hal_memset(&msg, 0, sizeof(msg_t));

	/* TODO: add mtSync message type in kernel */
	msg.type = 0xf52; /* mtSync */

	/* TODO: allow passing f->oid.id in mtSync message */
	/* Please note that we sync whole fs/dev for now */
	err = proc_send(f->oid.port, &msg);

	posix_fileDeref(f);

	return err;
}


static int posix_fcntlDup(int fd, int fd2, int cloexec)
{
	process_info_t *p;
	int err;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	if (fd < 0 || fd > p->maxfd || fd2 < 0 || fd > p->maxfd) {
		proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	while (p->fds[fd2].file != NULL && fd2++ < p->maxfd);

	if ((err = _posix_dup2(p, fd, fd2)) == fd2 && cloexec)
		p->fds[fd2].flags = FD_CLOEXEC;

	proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


static int posix_fcntlSetFd(int fd, unsigned flags)
{
	process_info_t *p;
	int err = EOK;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	proc_lockSet(&p->lock);
	if (fd < 0 || fd > p->maxfd) {
		proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	if (p->fds[fd].file != NULL)
		p->fds[fd].flags = flags;
	else
		err = -EBADF;
	proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


static int posix_fcntlGetFd(int fd)
{
	process_info_t *p;
	int err;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	proc_lockSet(&p->lock);
	if (fd < 0 || fd > p->maxfd) {
		proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	if (p->fds[fd].file != NULL)
		err = p->fds[fd].flags;
	else
		err = -EBADF;
	proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


static int posix_fcntlSetFl(int fd, int val)
{
	open_file_t *f;
	int err = EOK;
	/* Creation and access mode flags shall be ignored */
	int ignorefl = O_CREAT|O_EXCL|O_NOCTTY|O_TRUNC|O_RDONLY|O_RDWR|O_WRONLY;

	if (!(err = posix_getOpenFile(fd, &f))) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_setfl(f->oid.port, val);
				break;
			case ftUnixSocket:
				err = unix_setfl(f->oid.id, val);
				break;
			default:
				f->status = (val & ~ignorefl) | (f->status & ignorefl);
				break;
		}

		posix_fileDeref(f);
	}

	return err;
}


static int posix_fcntlGetFl(int fd)
{
	open_file_t *f;
	int err = EOK;

	if (!(err = posix_getOpenFile(fd, &f))) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_getfl(f->oid.port);
				break;
			case ftUnixSocket:
				err = unix_getfl(f->oid.id);
				break;
			default:
				err = f->status;
				break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_fcntl(int fd, unsigned int cmd, char *ustack)
{
	TRACE("fcntl(%d, %u)", fd, cmd);

	int err = -EINVAL, fd2;
	unsigned long arg;
	int cloexec = 0;

	switch (cmd) {
	case F_DUPFD_CLOEXEC:
		cloexec = 1;
	case F_DUPFD:
		GETFROMSTACK(ustack, int, fd2, 2);
		err = posix_fcntlDup(fd, fd2, cloexec);
		break;

	case F_GETFD:
		err = posix_fcntlGetFd(fd);
		break;

	case F_SETFD:
		GETFROMSTACK(ustack, unsigned long, arg, 2);
		err = posix_fcntlSetFd(fd, arg);
		break;

	case F_GETFL:
		err = posix_fcntlGetFl(fd);
		break;

	case F_SETFL:
		GETFROMSTACK(ustack, unsigned int, arg, 2);
		err = posix_fcntlSetFl(fd, arg);
		break;

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		/* TODO: implement */
		err = EOK;
	case F_GETOWN:
	case F_SETOWN:
	default:
		break;
	}

	return err;
}


#define IOCPARM_MASK		0x1fff
#define IOCPARM_LEN(x)		(((x) >> 16) & IOCPARM_MASK)


#define IOC_OUT				0x40000000
#define IOC_IN				0x80000000
#define IOC_INOUT			(IOC_IN | IOC_OUT)

#define _IOC(inout,group,num,len)	((unsigned long) (inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num)))
#define SIOCGIFCONF			_IOC(IOC_INOUT, 'S', 0x12, sizeof(struct ifconf))
#define SIOCADDRT			_IOC(IOC_IN, 'S', 0x44, sizeof(struct rtentry))
#define SIOCDELRT			_IOC(IOC_IN, 'S', 0x45, sizeof(struct rtentry))


void ioctl_pack(msg_t *msg, unsigned long request, void *data, id_t id)
{
	size_t size = IOCPARM_LEN(request);
	ioctl_in_t *ioctl = (ioctl_in_t *)msg->i.raw;

	msg->type = mtDevCtl;
	msg->i.data = NULL;
	msg->i.size = 0;
	msg->o.data = NULL;
	msg->o.size = 0;

	ioctl->request = request;
	ioctl->id = id;
	ioctl->pid = proc_current()->process->id;

	if (request & IOC_INOUT) {
		if (request & IOC_IN) {
			if (size <= (sizeof(msg->i.raw) - sizeof(ioctl_in_t))) {
				hal_memcpy(ioctl->data, data, size);
			} else {
				msg->i.data = data;
				msg->i.size = size;
			}
		}

		if ((request & IOC_OUT) && size > (sizeof(msg->o.raw) - sizeof(ioctl_out_t))) {
			msg->o.data = data;
			msg->o.size = size;
		}
	} else if (size > 0) {
		/* the data is passed by value instead of pointer */
		size = min(size, sizeof(void*));
		hal_memcpy(ioctl->data, &data, size);
	}

	/* ioctl special case: arg is structure with pointer - has to be custom-packed into message */
	if (request == SIOCGIFCONF) {
		struct ifconf *ifc = (struct ifconf*) data;
		msg->o.data = ifc->ifc_buf;
		msg->o.size = ifc->ifc_len;
	}
	else if (request == SIOCADDRT || request == SIOCDELRT) {
		struct rtentry *rt = (struct rtentry *)data;
		if (rt->rt_dev != NULL) {
			msg->o.data = rt->rt_dev;
			msg->o.size = hal_strlen(rt->rt_dev) + 1;
		}
	}
}


int ioctl_processResponse(const msg_t *msg, unsigned long request, void *data)
{
	size_t size = IOCPARM_LEN(request);
	int err;
	ioctl_out_t *ioctl = (ioctl_out_t *)msg->o.raw;

	err = ioctl->err;

	if ((request & IOC_OUT) && size <= (sizeof(msg->o.raw) - sizeof(ioctl_out_t))) {
		hal_memcpy(data, ioctl->data, size);
	}

	if (request == SIOCGIFCONF) { // restore overriden userspace pointer
		struct ifconf* ifc = (struct ifconf*) data;
		ifc->ifc_buf = msg->o.data;
	}

	return err;
}


int posix_ioctl(int fildes, unsigned long request, char *ustack)
{
	TRACE("ioctl(%d, %d)", fildes, request);

	open_file_t *f;
	int err;
	msg_t msg;
	void *data = NULL;

	if (!(err = posix_getOpenFile(fildes, &f))) {
		switch (request) {
			/* TODO: handle POSIX defined requests */
		default:
			if ((request & IOC_INOUT) || (IOCPARM_LEN(request) > 0))
				GETFROMSTACK(ustack, void *, data, 2);

			ioctl_pack(&msg, request, data, f->oid.id);

			if ((err = proc_send(f->oid.port, &msg)) == EOK)
				err = ioctl_processResponse(&msg, request, data);
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_socket(int domain, int type, int protocol)
{
	TRACE("socket(%d, %d, %d)", domain, type, protocol);

	process_info_t *p;
	int err, fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	if ((fd = posix_newFile(p, 0)) < 0) {
		pinfo_put(p);
		return -EMFILE;
	}

	switch (domain) {
		case AF_UNIX:
			if ((err = unix_socket(domain, type, protocol)) >= 0) {
				p->fds[fd].file->type = ftUnixSocket;
				p->fds[fd].file->oid.port = -1;
				p->fds[fd].file->oid.id = err;
			}
			break;
		case AF_INET:
		case AF_INET6:
		case AF_KEY:
		case AF_PACKET:
			if ((err = inet_socket(domain, type, protocol)) >= 0) {
				p->fds[fd].file->type = ftInetSocket;
				p->fds[fd].file->oid.port = err;
				p->fds[fd].file->oid.id = 0;
			}
			break;
		default:
			err = -EAFNOSUPPORT;
			break;
	}

	if (err < 0) {
		posix_putUnusedFile(p, fd);
		pinfo_put(p);
		return err;
	}

	if (type & SOCK_CLOEXEC)
		p->fds[fd].flags = FD_CLOEXEC;

	pinfo_put(p);
	return fd;
}


int posix_socketpair(int domain, int type, int protocol, int sv[2])
{
	TRACE("socketpair(%d, %d, %d, %p)", domain, type, protocol, sv);

	process_info_t *p;
	int err, id[2];

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	if (domain != AF_UNIX)
		return -EAFNOSUPPORT;

	if ((sv[0] = posix_newFile(p, 0)) < 0) {
		pinfo_put(p);
		return -EMFILE;
	}

	if ((sv[1] = posix_newFile(p, 0)) < 0) {
		posix_putUnusedFile(p, sv[0]);
		pinfo_put(p);
		return -EMFILE;
	}

	if (!(err = unix_socketpair(domain, type, protocol, id))) {
		p->fds[sv[0]].file->type = ftUnixSocket;
		p->fds[sv[1]].file->type = ftUnixSocket;
		p->fds[sv[0]].file->oid.port = -1;
		p->fds[sv[1]].file->oid.port = -1;
		p->fds[sv[0]].file->oid.id = id[0];
		p->fds[sv[1]].file->oid.id = id[1];

		if (type & SOCK_CLOEXEC) {
			p->fds[sv[0]].flags = FD_CLOEXEC;
			p->fds[sv[1]].flags = FD_CLOEXEC;
		}
	}
	else {
		posix_putUnusedFile(p, sv[1]);
		posix_putUnusedFile(p, sv[0]);
	}

	pinfo_put(p);
	return err;
}


int posix_accept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags)
{
	TRACE("accept4(%d, %s, %d)", socket, address == NULL ? NULL : address->sa_data, flags);

	process_info_t *p;
	open_file_t *f;
	int err, fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	if ((fd = posix_newFile(p, 0)) < 0) {
		pinfo_put(p);
		return -EMFILE;
	}

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
			case ftInetSocket:
				if ((err = inet_accept4(f->oid.port, address, address_len, flags)) >= 0) {
					p->fds[fd].file->type = ftInetSocket;
					p->fds[fd].file->oid.port = err;
					p->fds[fd].file->oid.id = 0;
				}
				break;
			case ftUnixSocket:
				if ((err = unix_accept4(f->oid.id, address, address_len, flags)) >= 0) {
					p->fds[fd].file->type = ftUnixSocket;
					p->fds[fd].file->oid.port = -1;
					p->fds[fd].file->oid.id = err;
				}
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		posix_fileDeref(f);
	}

	if (err < 0) {
		posix_putUnusedFile(p, fd);
		pinfo_put(p);
		return err;
	}

	if (flags & SOCK_CLOEXEC)
		p->fds[fd].flags = FD_CLOEXEC;

	pinfo_put(p);
	return fd;
}


int posix_accept(int socket, struct sockaddr *address, socklen_t *address_len)
{
	return posix_accept4(socket, address, address_len, 0);
}


int posix_bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	TRACE("bind(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_bind(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_bind(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	TRACE("connect(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_connect(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_connect(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_gethostname(char *name, size_t namelen)
{
	TRACE("gethostname(%zu)", namelen);

	hal_strncpy(name, posix_common.hostname, namelen);

	return 0;
}


int posix_getpeername(int socket, struct sockaddr *address, socklen_t *address_len)
{
	TRACE("getpeername(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_getpeername(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_getpeername(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_getsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
	TRACE("getsockname(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_getsockname(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_getsockname(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
	TRACE("getsockopt(%d, %d, %d)", socket, level, optname);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_getsockopt(f->oid.port, level, optname, optval, optlen);
			break;
		case ftUnixSocket:
			err = unix_getsockopt(f->oid.id, level, optname, optval, optlen);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_listen(int socket, int backlog)
{
	TRACE("listen(%d, %d)", socket, backlog);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_listen(f->oid.port, backlog);
			break;
		case ftUnixSocket:
			err = unix_listen(f->oid.id, backlog);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_recvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	TRACE("recvfrom(%d, %d, %s)", socket, length, src_addr == NULL ? NULL : src_addr->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_recvfrom(f->oid.port, message, length, flags, src_addr, src_len);
			break;
		case ftUnixSocket:
			err = unix_recvfrom(f->oid.id, message, length, flags, src_addr, src_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	TRACE("sendto(%d, %s, %d, %s)", socket, message, length, dest_addr == NULL ? NULL : dest_addr->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_sendto(f->oid.port, message, length, flags, dest_addr, dest_len);
			break;
		case ftUnixSocket:
			err = unix_sendto(f->oid.id, message, length, flags, dest_addr, dest_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_recvmsg(int socket, struct msghdr *msg, int flags)
{
	TRACE("recvmsg(%d, %p, %d)", socket, msg, flags);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_recvmsg(f->oid.port, msg, flags);
				break;
			case ftUnixSocket:
				err = unix_recvmsg(f->oid.id, msg, flags);
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_sendmsg(int socket, const struct msghdr *msg, int flags)
{
	TRACE("sendmsg(%d, %p, %d)", socket, msg, flags);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_sendmsg(f->oid.port, msg, flags);
				break;
			case ftUnixSocket:
				err = unix_sendmsg(f->oid.id, msg, flags);
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_shutdown(int socket, int how)
{
	TRACE("shutdown(%d, %d)", socket, how);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_shutdown(f->oid.port, how);
			break;
		case ftUnixSocket:
			err = unix_shutdown(f->oid.id, how);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_sethostname(const char *name, size_t namelen)
{
	TRACE("sethostname(%zu)", namelen);

	if (namelen > HOST_NAME_MAX)
		return -EINVAL;

	hal_strncpy(posix_common.hostname, name, namelen);
	posix_common.hostname[namelen] = '\0';

	return 0;
}


int posix_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_setsockopt(f->oid.port, level, optname, optval, optlen);
			break;
		case ftUnixSocket:
			err = unix_setsockopt(f->oid.id, level, optname, optval, optlen);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		posix_fileDeref(f);
	}

	return err;
}


int posix_futimens(int fildes, const struct timespec *times)
{
	TRACE("futimens(%d)", fildes);

	open_file_t *f;
	msg_t msg;
	int err;

	err = posix_getOpenFile(fildes, &f);
	if (err < 0)
		return err;

	hal_memset(&msg, 0, sizeof(msg_t));

	msg.type = mtSetAttr;
	hal_memcpy(&msg.i.attr.oid, &f->oid, sizeof(oid_t));

	msg.i.attr.type = atMTime;
	msg.i.attr.val = times[1].tv_sec;
	err = proc_send(f->oid.port, &msg);
	if ((err >= 0) && (msg.o.attr.err >= 0)) {
		msg.i.attr.type = atATime;
		msg.i.attr.val = times[0].tv_sec;
		err = proc_send(f->oid.port, &msg);
	}
	if (err >= 0)
		err = msg.o.attr.err;

	posix_fileDeref(f);

	return err;
}


static int do_poll_iteration(struct pollfd *fds, nfds_t nfds)
{
	msg_t msg;
	size_t ready, i;
	int err;
	open_file_t *f;

	hal_memset(&msg, 0, sizeof(msg));

	msg.type = mtGetAttr;
	msg.i.attr.type = atPollStatus;

	for (ready = i = 0; i < nfds; ++i) {
		if (fds[i].fd < 0)
			continue;

		msg.i.attr.val = fds[i].events;

		if (posix_getOpenFile(fds[i].fd, &f) < 0) {
			err = POLLNVAL;
		}
		else {
			hal_memcpy(&msg.i.attr.oid, &f->oid, sizeof(oid_t));
			posix_fileDeref(f);

			if (f->type == ftUnixSocket)
				err = unix_poll(msg.i.attr.oid.id, fds[i].events);
			else if (((err = proc_send(msg.i.attr.oid.port, &msg)) == EOK) && ((err = msg.o.attr.err) == EOK))
				err = msg.o.attr.val;
		}

		if (err == -EINTR)
			return err;

		if (err < 0)
			fds[i].revents |= POLLHUP;
		else if (err > 0)
			fds[i].revents |= err;

		fds[i].revents &= ~(~fds[i].events & (POLLIN|POLLOUT|POLLPRI|POLLRDNORM|POLLWRNORM|POLLRDBAND|POLLWRBAND));

		if (fds[i].revents)
			++ready;
	}

	return ready;
}

#if 1
int posix_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms)
{
	size_t i, n, ready;
	time_t timeout, now;

	for (i = n = 0; i < nfds; ++i) {
		fds[i].revents = 0;
		if (fds[i].fd >= 0)
			++n;
	}

	if (!n) {
		if (timeout_ms > 0)
			proc_threadSleep(timeout_ms * 1000LL);
		return 0;
	}

	if (timeout_ms >= 0) {
		proc_gettime(&timeout, NULL);
		timeout += timeout_ms * 1000LL + !timeout_ms;
	} else
		timeout = 0;

	while (!(ready = do_poll_iteration(fds, nfds))) {
		if (timeout) {
			proc_gettime(&now, NULL);
			if (now > timeout)
				break;

			now = timeout - now;
			if (now > POLL_INTERVAL)
				now = POLL_INTERVAL;
		} else
			now = POLL_INTERVAL;

		proc_threadSleep(now);
	}

	return ready;
}

#else

int posix_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms)
{
	int err, i, j;
	int queue;
	msg_t msg;
	open_file_t *f, *q;

	evsub_t subs_stack[4];
	evsub_t *subs = subs_stack;
	event_t events[8];

	/* fast path */
	if ((err = do_poll_iteration(fds, nfds)))
		return err;
	else if (!timeout_ms)
		return 0;

	if ((queue = posix_open("/dev/event/queue", O_RDWR, NULL)) < 0)
		return queue;

	do {
		if (posix_getOpenFile(queue, &q) < 0)
			return -EAGAIN; /* should not happen? */

		if ((nfds > sizeof(subs_stack) / sizeof(evsub_t)) && (subs = vm_kmalloc(nfds * sizeof(evsub_t))) == NULL)
			return -ENOMEM;

		hal_memset(subs, 0, nfds * sizeof(evsub_t));

		do {
			err = EOK;

			for (i = 0; i < nfds; ++i) {
				if (fds[i].fd < 0)
					continue;

				if ((err = posix_getOpenFile(fds[i].fd, &f))) {
					fds[i].revents = POLLNVAL;
					continue;
				}

				hal_memcpy(&subs[i].oid, &f->oid, sizeof(oid_t));
				subs[i].flags = evAdd;
				subs[i].types = fds[i].events;
			}

			if (err)
				break;

			msg.type = mtRead;

			hal_memcpy(&msg.i.io.oid, &q->oid, sizeof(oid_t));
			msg.i.io.len = (unsigned)timeout_ms;

			msg.i.data = subs;
			msg.i.size = nfds * sizeof(evsub_t);

			msg.o.data = events;
			msg.o.size = sizeof(events);

			if ((err = proc_send(q->oid.port, &msg)))
				break;

			if ((err = msg.o.io.err) < 0)
				break;

			if (!err)
				break;

			for (i = 0; i < msg.o.io.err; ++i) {
				for (j = 0; j < nfds; ++j) {
					if (hal_memcmp(&events[i].oid, &subs[j].oid, sizeof(oid_t)))
						continue;

					fds[j].revents |= 1 << events[i].type;
				}
			}

		} while (0);

		if (subs != subs_stack)
			vm_kfree(subs);
	} while (0);

	posix_close(queue);
	return err;
}
#endif


static int posix_killOne(pid_t pid, int tid, int sig)
{
	process_info_t *pinfo;
	process_t *proc;
	thread_t *thr;
	int err;

	if ((pinfo = pinfo_find(pid)) != NULL || !(err = -EINVAL)) {
		if ((proc = proc_find(pinfo->process)) != NULL || !(err = -ESRCH)) {
			if (!tid) {
				err = threads_sigpost(proc, NULL, sig);
			}
			else if ((thr = threads_findThread(tid)) != NULL || !(err = -EINVAL)) {
				if (thr->process == proc || !(err = -EINVAL))
					err = threads_sigpost(proc, thr, sig);

				threads_put(thr);
			}
			proc_put(proc);
		}
		pinfo_put(pinfo);
	}

	return err;
}


static int posix_killGroup(pid_t pgid, int sig)
{
	process_info_t *pinfo;
	rbnode_t *node;

	proc_lockSet(&posix_common.lock);
	for (node = lib_rbMinimum(posix_common.pid.root); node != NULL; node = lib_rbNext(node)) {
		pinfo = lib_treeof(process_info_t, linkage, node);

		if (pinfo->pgid == pgid)
			proc_sigpost(pinfo->process, sig);
	}
	proc_lockClear(&posix_common.lock);

	return EOK;
}


int posix_tkill(pid_t pid, int tid, int sig)
{
	TRACE("tkill(%p, %d, %d)", pid, tid, sig);

	if (sig < 0 || sig > NSIG)
		return -EINVAL;

	/* TODO: handle pid = 0 */
	if (pid == 0)
		return -ENOSYS;

	if (pid == -1)
		return -ESRCH;

	if (pid > 0)
		return posix_killOne(pid, tid, sig);

	return posix_killGroup(-pid, sig);
}


void posix_sigchild(pid_t ppid)
{
	posix_tkill(ppid, NULL, SIGCHLD);
}


int posix_setpgid(pid_t pid, pid_t pgid)
{
	process_info_t *pinfo;

	if (pid < 0 || pgid < 0)
		return -EINVAL;

	if (!pid)
		pid = proc_current()->process->id;

	if (!pgid)
		pgid = pid;

	if ((pinfo = pinfo_find(pid)) == NULL)
		return -EINVAL;

	proc_lockSet(&pinfo->lock);
	pinfo->pgid = pgid;
	proc_lockClear(&pinfo->lock);
	pinfo_put(pinfo);
	return EOK;
}


pid_t posix_getpgid(pid_t pid)
{
	process_info_t *pinfo;
	pid_t res;

	if (pid < 0)
		return -EINVAL;

	if (!pid)
		pid = proc_current()->process->id;

	if ((pinfo = pinfo_find(pid)) == NULL)
		return -EINVAL;

	proc_lockSet(&pinfo->lock);
	res = pinfo->pgid;
	proc_lockClear(&pinfo->lock);
	pinfo_put(pinfo);

	return res;
}


pid_t posix_setsid(void)
{
	process_info_t *pinfo;
	pid_t pid;

	pid = proc_current()->process->id;

	if ((pinfo = pinfo_find(pid)) == NULL)
		return -EINVAL;

	/* FIXME (pedantic): Should check if any process has my group id */
	proc_lockSet(&pinfo->lock);
	if (pinfo->pgid == pid) {
		proc_lockClear(&pinfo->lock);
		pinfo_put(pinfo);
		return -EPERM;
	}

	pinfo->pgid = pid;
	proc_lockClear(&pinfo->lock);
	pinfo_put(pinfo);

	return EOK;
}


int posix_waitpid(pid_t child, int *status, int options)
{
	process_info_t *pinfo, *c;
	pid_t pid;
	int err = EOK;
	int found = 0;

	pid = proc_current()->process->id;

	if ((pinfo = pinfo_find(pid)) == NULL)
		return -EINVAL;

	proc_lockSet(&pinfo->lock);
	do {
		if ((c = pinfo->zombies) == NULL) {
			if (pinfo->children == NULL) {
				err = -ECHILD;
				break;
			}
			else if (options & 1) {
				err = EOK;
				break;
			}
			else {
				while ((c = pinfo->zombies) == NULL && !err)
					err = proc_lockWait(&pinfo->wait, &pinfo->lock, 0);

				if (err == -EINTR) {
					/* pinfo->lock is clear */
					pinfo_put(pinfo);
					return -EINTR;
				}
				else if (err) {
					/* Should not happen */
					break;
				}
			}
		}

		do {
			if (child == -1 || (!child && c->pgid == pinfo->pgid) || (child < 0 && c->pgid == -child) || child == c->process) {
				LIST_REMOVE(&pinfo->zombies, c);
				found = 1;
			}
		}
		while (!found && (c = c->next) != pinfo->zombies);
	}
	while (!found && !(options & 1));
	proc_lockClear(&pinfo->lock);

	if (found) {
		err = c->process;

		if (status != NULL)
			*status = c->exitcode;

		pinfo_put(c);
	}

	pinfo_put(pinfo);
	return err;
}


void posix_died(pid_t pid, int exit)
{
	process_info_t *pinfo, *ppinfo;

	if ((pinfo = pinfo_find(pid)) == NULL)
		return;

	posix_exit(pinfo, exit);

	if ((ppinfo = pinfo_find(pinfo->parent)) == NULL) {
		// posix_destroy(pinfo);
	}
	else {
		proc_lockSet(&ppinfo->lock);
		LIST_REMOVE(&ppinfo->children, pinfo);
		LIST_ADD(&ppinfo->zombies, pinfo);
		proc_threadBroadcast(&ppinfo->wait);
		proc_lockClear(&ppinfo->lock);

		posix_sigchild(pinfo->parent);

		pinfo_put(ppinfo);
	}

	pinfo_put(pinfo);
}


pid_t posix_getppid(pid_t pid)
{
	process_info_t *pinfo;
	int ret = 0;

	if ((pinfo = pinfo_find(pid)) == NULL)
		return -ENOSYS;

	ret = pinfo->parent;

	pinfo_put(pinfo);

	return ret;
}


void posix_init(void)
{
	proc_lockInit(&posix_common.lock);
	lib_rbInit(&posix_common.pid, pinfo_cmp, NULL);
	unix_sockets_init();
	posix_common.fresh = 0;
	hal_memset(posix_common.hostname, 0, sizeof(posix_common.hostname));
}
