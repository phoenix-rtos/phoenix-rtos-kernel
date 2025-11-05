/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module
 *
 * Copyright 2018, 2023 Phoenix Systems
 * Author: Jan Sikorski, Michal Miroslaw, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "include/errno.h"
#include "include/events.h"
#include "include/file.h"
#include "include/ioctl.h"
#include "include/limits.h"
#include "include/posix-fcntl.h"

#include "proc/proc.h"

#include "posix_private.h"
#include "lib/lib.h"

#define MAX_FD_COUNT     1024
#define INITIAL_FD_COUNT 32

#if 0 /* Debug */
#define TRACE(str, ...) lib_printf("posix %x: " str "\n", proc_current()->process->id, ##__VA_ARGS__)
#else
#define TRACE(str, ...)
#endif

#define POLL_INTERVAL 100000


typedef struct {
	oid_t oid;
	unsigned int flags;
	unsigned short types;
} evsub_t;


typedef struct _event_t {
	oid_t oid;
	unsigned int type;

	unsigned int flags;
	unsigned int count;
	unsigned int data;
} event_t;


static struct {
	rbtree_t pid;
	lock_t lock;
	id_t fresh;
	char hostname[HOST_NAME_MAX + 1U];
} posix_common;


static process_info_t *_pinfo_find(int pid)
{
	process_info_t pi, *r;

	pi.process = pid;
	r = lib_treeof(process_info_t, linkage, lib_rbFind(&posix_common.pid, &pi.linkage));
	if (r != NULL) {
		r->refs++;
	}

	return r;
}


process_info_t *pinfo_find(int pid)
{
	process_info_t *r;

	(void)proc_lockSet(&posix_common.lock);
	r = _pinfo_find(pid);
	(void)proc_lockClear(&posix_common.lock);
	return r;
}


void pinfo_put(process_info_t *p)
{
	(void)proc_lockSet(&posix_common.lock);
	p->refs--;
	if (p->refs != 0) {
		(void)proc_lockClear(&posix_common.lock);
		return;
	}

	lib_rbRemove(&posix_common.pid, &p->linkage);
	(void)proc_lockClear(&posix_common.lock);

	vm_kfree(p->fds);
	(void)proc_lockDone(&p->lock);
	vm_kfree(p);
}


int posix_fileDeref(open_file_t *f)
{
	int err = EOK;

	(void)proc_lockSet(&f->lock);
	--f->refs;
	if (f->refs == 0U) {
		if (f->type == ftUnixSocket) {
			err = unix_close(f->oid.id);
		}
		else {
			do {
				err = proc_close(f->oid, f->status);
			} while (err == -EINTR);
		}

		(void)proc_lockDone(&f->lock);
		vm_kfree(f);
	}
	else {
		(void)proc_lockClear(&f->lock);
	}
	return err;
}


static void posix_putUnusedFile(process_info_t *p, int fd)
{
	open_file_t *f;

	f = p->fds[fd].file;
	(void)proc_lockDone(&f->lock);
	vm_kfree(f);
	p->fds[fd].file = NULL;
}


int posix_getOpenFile(int fd, open_file_t **f)
{
	process_info_t *p;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -ENOSYS;
	}

	(void)proc_lockSet(&p->lock);
	if ((fd < 0) || (fd >= p->fdsz) || (p->fds[fd].file == NULL)) {
		(void)proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	*f = p->fds[fd].file;

	(void)proc_lockSet(&(*f)->lock);
	(*f)->refs++;
	(void)proc_lockClear(&(*f)->lock);
	(void)proc_lockClear(&p->lock);

	pinfo_put(p);
	return 0;
}


static int _posix_allocfd(process_info_t *p, int fd)
{
	fildes_t *nfds;
	int nfdsz = p->fdsz;

	for (; fd < p->maxfd; ++fd) {
		if (fd >= p->fdsz) {
			while (fd >= nfdsz) {
				nfdsz *= 2;
			}

			if (nfdsz > p->maxfd) {
				/* fd can't be >= p->maxfd, so it's always ok */
				nfdsz = p->maxfd;
			}

			nfds = vm_kmalloc((size_t)nfdsz * sizeof(*nfds));
			if (nfds == NULL) {
				return -1;
			}

			hal_memcpy(nfds, p->fds, (size_t)p->fdsz * sizeof(*nfds));
			hal_memset(nfds + p->fdsz, 0, ((size_t)nfdsz - (size_t)p->fdsz) * sizeof(*nfds));

			vm_kfree(p->fds);

			p->fds = nfds;
			p->fdsz = nfdsz;
		}

		if (p->fds[fd].file == NULL) {
			return fd;
		}
	}

	return -1;
}


int posix_newFile(process_info_t *p, int fd)
{
	open_file_t *f;

	f = vm_kmalloc(sizeof(open_file_t));
	if (f == NULL) {
		return -ENOMEM;
	}

	(void)proc_lockSet(&p->lock);

	fd = _posix_allocfd(p, fd);
	if (fd < 0) {
		(void)proc_lockClear(&p->lock);
		vm_kfree(f);
		return -ENFILE;
	}

	p->fds[fd].file = f;

	hal_memset(f, 0, sizeof(open_file_t));
	f->refs = 1;
	f->offset = 0;
	(void)proc_lockInit(&f->lock, &proc_lockAttrDefault, "posix.file");
	(void)proc_lockClear(&p->lock);
	return fd;
}


int _posix_addOpenFile(process_info_t *p, open_file_t *f, unsigned int flags)
{
	int fd = 0;

	fd = _posix_allocfd(p, fd);
	if (fd < 0) {
		return -ENFILE;
	}

	p->fds[fd].file = f;
	p->fds[fd].flags = flags;

	return fd;
}


static int pinfo_cmp(rbnode_t *n1, rbnode_t *n2)
{
	process_info_t *p1 = lib_treeof(process_info_t, linkage, n1);
	process_info_t *p2 = lib_treeof(process_info_t, linkage, n2);

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	if (p1->process < p2->process) {
		return -1;
	}
	else if (p1->process > p2->process) {
		return 1;
	}
	else {
		return 0;
	}
}


static int posix_truncate(oid_t *oid, off_t length)
{
	msg_t msg;
	int err = -EINVAL;

	if ((oid->port != US_PORT) && (length >= 0)) {
		hal_memset(&msg, 0, sizeof(msg));
		msg.type = mtTruncate;
		hal_memcpy(&msg.oid, oid, sizeof(oid_t));
		msg.i.io.len = (size_t)length;
		err = proc_send(oid->port, &msg);
	}

	return err;
}


int posix_clone(int ppid)
{
	TRACE("clone(%x)", ppid);

	process_info_t *p, *pp;
	process_t *proc;
	int i, j;
	oid_t console;
	open_file_t *f;

	proc = proc_current()->process;

	p = vm_kmalloc(sizeof(process_info_t));
	if (p == NULL) {
		return -ENOMEM;
	}

	hal_memset(&console, 0, sizeof(console));
	(void)proc_lockInit(&p->lock, &proc_lockAttrDefault, "posix.process");
	p->children = NULL;
	p->zombies = NULL;
	p->wait = NULL;
	p->next = p->prev = NULL;
	p->refs = 1;

	pp = pinfo_find(ppid);
	if (pp != NULL) {
		TRACE("clone: got parent");
		(void)proc_lockSet(&pp->lock);
		p->maxfd = pp->maxfd;
		p->fdsz = pp->fdsz;
		LIST_ADD(&pp->children, p);
		p->parent = ppid;
	}
	else {
		p->parent = 0;
		p->maxfd = MAX_FD_COUNT;
		p->fdsz = INITIAL_FD_COUNT;
	}

	p->process = process_getPid(proc);

	p->fds = vm_kmalloc((unsigned int)p->fdsz * sizeof(fildes_t));
	if (p->fds == NULL) {
		(void)proc_lockDone(&p->lock);
		vm_kfree(p);
		if (pp != NULL) {
			(void)proc_lockClear(&pp->lock);
			pinfo_put(pp);
		}
		return -ENOMEM;
	}

	if (pp != NULL) {
		hal_memcpy(p->fds, pp->fds, (unsigned int)pp->fdsz * sizeof(fildes_t));

		for (i = 0; i < p->fdsz; ++i) {
			f = p->fds[i].file;
			if (f != NULL) {
				(void)proc_lockSet(&f->lock);
				++f->refs;
				(void)proc_lockClear(&f->lock);
			}
		}

		(void)proc_lockClear(&pp->lock);
	}
	else {
		hal_memset(p->fds, 0, (unsigned int)p->fdsz * sizeof(fildes_t));

		for (i = 0; i < 3; ++i) {
			f = vm_kmalloc(sizeof(open_file_t));
			p->fds[i].file = f;
			if (f == NULL) {
				for (j = 0; j < i; j++) {
					posix_putUnusedFile(p, j);
				}
				(void)proc_lockDone(&p->lock);
				vm_kfree(p->fds);
				vm_kfree(p);
				return -ENOMEM;
			}

			(void)proc_lockInit(&f->lock, &proc_lockAttrDefault, "posix.file");
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
		p->pgid = pp->pgid;
		pinfo_put(pp);
	}
	else {
		p->pgid = p->process;
	}

	(void)proc_lockSet(&posix_common.lock);
	(void)lib_rbInsert(&posix_common.pid, &p->linkage);
	(void)proc_lockClear(&posix_common.lock);

	return EOK;
}


int posix_exec(void)
{
	TRACE("exec()");

	process_info_t *p;
	int fd;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	(void)proc_lockSet(&p->lock);
	for (fd = 0; fd < p->fdsz; ++fd) {
		if ((p->fds[fd].file != NULL) && ((p->fds[fd].flags & FD_CLOEXEC) != 0U)) {
			(void)posix_fileDeref(p->fds[fd].file);
			p->fds[fd].file = NULL;
		}
	}
	(void)proc_lockClear(&p->lock);

	pinfo_put(p);
	return 0;
}


static int posix_exit(process_info_t *p, int code)
{
	int fd;

	p->exitcode = code;

	(void)proc_lockSet(&p->lock);
	for (fd = 0; fd < p->fdsz; ++fd) {
		if (p->fds[fd].file != NULL) {
			(void)posix_fileDeref(p->fds[fd].file);
		}
	}
	(void)proc_lockClear(&p->lock);

	return 0;
}


static int posix_create(const char *filename, int type, mode_t mode, oid_t dev, oid_t *oid)
{
	TRACE("posix_create(%s, %d)", filename, mode);

	int err;
	oid_t dir;
	char *name, *basename;
	const char *dirname;

	name = lib_strdup(filename);
	if (name == NULL) {
		return -ENOMEM;
	}

	lib_splitname(name, &basename, &dirname);

	do {
		err = proc_lookup(dirname, NULL, &dir);
		if (err < 0) {
			break;
		}

		err = proc_create(dir.port, type, mode, dev, dir, basename, oid);
		if (err < 0) {
			break;
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	return err;
}

int posix_statvfs(const char *path, int fildes, struct statvfs *buf)
{
	oid_t oid, dev;
	oid_t *oidp, *devp;
	open_file_t *f;
	msg_t msg;
	int err = EOK;

	if (((path == NULL) && (fildes < 0)) ||
			((path != NULL) && (fildes != -1))) {
		return -EINVAL;
	}

	if (path == NULL) {
		err = posix_getOpenFile(fildes, &f);
		if (err < 0) {
			return err;
		}
		oidp = &f->oid;
		devp = NULL;
	}
	else {
		if (proc_lookup(path, &oid, &dev) < 0) {
			return -ENOENT;
		}
		oidp = &oid;
		devp = &dev;
	}

	/* Detect mountpoint */
	if ((devp != NULL) && (oidp->port != devp->port)) {
		hal_memset(&msg, 0, sizeof(msg));
		msg.type = mtGetAttr;
		hal_memcpy(&msg.oid, oidp, sizeof(*oidp));
		msg.i.attr.type = atMode;

		if ((proc_send(oidp->port, &msg) < 0) || (msg.o.err < 0)) {
			return -EIO;
		}

		if (S_ISDIR((unsigned long long)msg.o.attr.val)) {
			oidp = devp;
		}
	}

	hal_memset(buf, 0, sizeof(*buf));

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = mtStat;
	msg.o.data = buf;
	msg.o.size = sizeof(*buf);

	if (proc_send(oidp->port, &msg) < 0) {
		err = -EIO;
	}
	else {
		err = msg.o.err;
	}

	if (path == NULL) {
		if (err == EOK) {
			err = posix_fileDeref(f);
		}
		else {
			(void)posix_fileDeref(f);
		}
	}

	return err;
}


/* TODO: handle O_CREAT and O_EXCL */
int posix_open(const char *filename, int oflag, u8 *ustack)
{
	TRACE("open(%s, %d, %d)", filename, oflag);
	oid_t ln, oid, dev, pipesrv;
	int fd = 0, err = 0;
	process_info_t *p;
	open_file_t *f;
	mode_t mode;

	if (proc_lookup("/dev/posix/pipes", NULL, &pipesrv) < 0) {
		hal_memset(&pipesrv, 0xff, sizeof(oid_t));
	}

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	hal_memset(&dev, 0, sizeof(oid_t));

	(void)proc_lockSet(&p->lock);

	do {
		fd = _posix_allocfd(p, fd);
		if (fd < 0) {
			err = -EBADF;
			break;
		}

		f = vm_kmalloc(sizeof(open_file_t));
		if (f == NULL) {
			err = -ENOMEM;
			break;
		}

		p->fds[fd].file = f;
		(void)proc_lockInit(&f->lock, &proc_lockAttrDefault, "posix.file");
		(void)proc_lockClear(&p->lock);

		do {
			err = proc_lookup(filename, &ln, &oid);
			if ((err == -ENOENT) && (((unsigned int)oflag & O_CREAT) != 0U)) {
				GETFROMSTACK(ustack, mode_t, mode, 2);

				if (posix_create(filename, 1 /* otFile */, mode | (unsigned int)S_IFREG, dev, &oid) < 0) {
					err = -EIO;
					break;
				}
				hal_memcpy(&ln, &oid, sizeof(oid_t));
			}
			else if (err < 0) {
				break;
			}
			else {
				/* No action required */
			}

			if (oid.port != US_PORT) {
				err = proc_open(oid, (unsigned int)oflag);
				if (err < 0) {
					break;
				}
			}

			(void)proc_lockSet(&p->lock);
			p->fds[fd].flags = ((unsigned int)oflag & O_CLOEXEC) != 0U ? FD_CLOEXEC : 0U;
			(void)proc_lockClear(&p->lock);

			if (err == 0) {
				hal_memcpy(&f->oid, &oid, sizeof(oid));
			}
			else {
				/* multiplexer, e.g. /dev/ptmx */
				f->oid.port = oid.port;
				f->oid.id = (unsigned int)err;
				/* FIXME Error can also be assign to -EINVAL
				 * which is -22. How to handle that?
				 */
			}

			hal_memcpy(&f->ln, &ln, sizeof(ln));

			f->refs = 1;

			/* TODO: check for other types */
			if (oid.port == US_PORT) {
				f->type = ftUnixSocket;
			}
			else if (oid.port == pipesrv.port) {
				f->type = ftPipe;
			}
			else {
				f->type = ftRegular;
			}

			if (((unsigned int)oflag & O_APPEND) != 0U) {
				f->offset = proc_size(f->oid);
			}
			else {
				f->offset = 0;
			}

			if (((unsigned int)oflag & O_TRUNC) != 0U) {
				(void)posix_truncate(&f->oid, 0);
			}

			f->status = (unsigned int)oflag & ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_CLOEXEC);

			pinfo_put(p);
			return fd;
		} while (0);

		(void)proc_lockSet(&p->lock);
		p->fds[fd].file = NULL;
		(void)proc_lockDone(&f->lock);
		vm_kfree(f);

	} while (0);

	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


int posix_close(int fildes)
{
	TRACE("close(%d)", fildes);
	open_file_t *f;
	process_info_t *p;
	int err = -EBADF;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	(void)proc_lockSet(&p->lock);

	do {
		if ((fildes < 0) || (fildes >= p->fdsz)) {
			break;
		}

		if (p->fds[fildes].file == NULL) {
			break;
		}

		f = p->fds[fildes].file;
		p->fds[fildes].file = NULL;
		(void)proc_lockClear(&p->lock);

		pinfo_put(p);
		return posix_fileDeref(f);
	} while (0);

	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


ssize_t posix_read(int fildes, void *buf, size_t nbyte, off_t offset)
{
	TRACE("read(%d, %p, %zu, %jd)", fildes, buf, nbyte, (intmax_t)offset);

	open_file_t *f;
	ssize_t rcnt;
	off_t offs = offset;
	unsigned int status;
	int err;

	err = posix_getOpenFile(fildes, &f);
	if (err < 0) {
		return err;
	}

	if ((f->status & O_WRONLY) != 0U) {
		(void)posix_fileDeref(f);
		return -EBADF;
	}

	if (offset >= 0 && !F_SEEKABLE(f->type)) {
		(void)posix_fileDeref(f);
		return -ESPIPE;
	}

	(void)proc_lockSet(&f->lock);
	/* offset < 0 means use current fd offset */
	if (offset < 0) {
		offs = f->offset;
	}
	status = f->status;
	(void)proc_lockClear(&f->lock);

	if (f->type == ftUnixSocket) {
		rcnt = unix_recvfrom(f->oid.id, buf, nbyte, 0, NULL, NULL);
	}
	else {
		rcnt = proc_read(f->oid, offs, buf, nbyte, status);
	}

	if (rcnt > 0 && offset < 0) {
		(void)proc_lockSet(&f->lock);
		f->offset += rcnt;
		(void)proc_lockClear(&f->lock);
	}

	(void)posix_fileDeref(f);

	return rcnt;
}


ssize_t posix_write(int fildes, void *buf, size_t nbyte, off_t offset)
{
	TRACE("write(%d, %p, %zu, %jd)", fildes, buf, nbyte, (intmax_t)offset);

	open_file_t *f;
	ssize_t rcnt;
	off_t offs = offset;
	unsigned int status;
	int err;

	err = posix_getOpenFile(fildes, &f);
	if (err < 0) {
		return err;
	}

	if ((f->status & O_RDONLY) != 0U) {
		(void)posix_fileDeref(f);
		return -EBADF;
	}

	if (offset >= 0 && !F_SEEKABLE(f->type)) {
		(void)posix_fileDeref(f);
		return -ESPIPE;
	}

	(void)proc_lockSet(&f->lock);
	/* offset < 0 means use current fd offset */
	if (offset < 0) {
		offs = f->offset;
	}
	status = f->status;
	(void)proc_lockClear(&f->lock);

	if (f->type == ftUnixSocket) {
		rcnt = unix_sendto(f->oid.id, buf, nbyte, 0, NULL, 0);
	}
	else {
		rcnt = proc_write(f->oid, offs, buf, nbyte, status);
	}

	if (rcnt > 0 && offset < 0) {
		(void)proc_lockSet(&f->lock);
		f->offset += rcnt;
		(void)proc_lockClear(&f->lock);
	}

	(void)posix_fileDeref(f);

	return rcnt;
}


int posix_getOid(int fildes, oid_t *oid)
{
	open_file_t *f;
	int err;

	err = posix_getOpenFile(fildes, &f);
	if (err < 0) {
		return err;
	}

	hal_memcpy(oid, &f->oid, sizeof(oid_t));

	(void)posix_fileDeref(f);

	return EOK;
}


int posix_dup(int fildes)
{
	TRACE("dup(%d)", fildes);

	process_info_t *p;
	int newfd = 0;
	open_file_t *f;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	(void)proc_lockSet(&p->lock);

	do {
		if ((fildes < 0) || (fildes >= p->fdsz)) {
			break;
		}

		if (p->fds[fildes].file == NULL) {
			break;
		}

		f = p->fds[fildes].file;
		newfd = _posix_allocfd(p, newfd);
		if (newfd < 0) {
			break;
		}

		p->fds[newfd].file = f;
		p->fds[newfd].flags = 0;
		(void)proc_lockSet(&f->lock);
		f->refs++;
		(void)proc_lockClear(&f->lock);
		(void)proc_lockClear(&p->lock);
		pinfo_put(p);

		return newfd;
	} while (0);

	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return -EBADF;
}


static int _posix_dup2(process_info_t *p, int fildes, int fildes2)
{
	open_file_t *f, *f2;
	int nfd2;

	if ((fildes < 0) || (fildes >= p->fdsz)) {
		return -EBADF;
	}

	if ((fildes2 < 0) || (fildes2 >= p->maxfd)) {
		return -EBADF;
	}

	if (p->fds[fildes].file == NULL) {
		return -EBADF;
	}

	if (fildes == fildes2) {
		return fildes2;
	}

	if (fildes2 >= p->fdsz) {
		/* requested fd bigger than current table, resize to match */
		nfd2 = _posix_allocfd(p, fildes2);

		/* sanity check */
		if (nfd2 != fildes2) {
			return -EFAULT;
		}
	}

	f = p->fds[fildes].file;
	f2 = p->fds[fildes2].file;

	if (p->fds[fildes2].file != NULL) {
		p->fds[fildes2].file = NULL;
		(void)posix_fileDeref(f2);
	}

	p->fds[fildes2].file = f;
	p->fds[fildes2].flags = 0;

	(void)proc_lockSet(&f->lock);
	f->refs++;
	(void)proc_lockClear(&f->lock);

	return fildes2;
}


int posix_dup2(int fildes, int fildes2)
{
	TRACE("dup2(%d, %d)", fildes, fildes2);

	process_info_t *p;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	(void)proc_lockSet(&p->lock);
	fildes2 = _posix_dup2(p, fildes, fildes2);
	(void)proc_lockClear(&p->lock);
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

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	hal_memset(&oid, 0, sizeof(oid));

	res = proc_lookup("/dev/posix/pipes", NULL, &pipesrv);
	if (res < 0) {
		pinfo_put(p);
		return (res == -EINTR) ? res : -ENOSYS;
	}

	res = proc_create(pipesrv.port, pxBufferedPipe, O_RDONLY | O_WRONLY, oid, pipesrv, NULL, &oid);
	if (res < 0) {
		pinfo_put(p);
		return res;
	}

	fo = vm_kmalloc(sizeof(open_file_t));
	if (fo == NULL) {
		pinfo_put(p);
		/* FIXME: destroy pipe */
		return -ENOMEM;
	}

	fi = vm_kmalloc(sizeof(open_file_t));
	if (fi == NULL) {
		vm_kfree(fo);
		pinfo_put(p);
		/* FIXME: destroy pipe */
		return -ENOMEM;
	}

	(void)proc_lockSet(&p->lock);
	fildes[0] = _posix_allocfd(p, 0);
	if (fildes[0] >= 0) {
		fildes[1] = _posix_allocfd(p, fildes[0] + 1);
	}

	if ((fildes[0] < 0) || (fildes[1] < 0)) {
		(void)proc_lockClear(&p->lock);

		vm_kfree(fo);
		vm_kfree(fi);

		pinfo_put(p);
		return -EMFILE;
	}

	p->fds[fildes[0]].flags = p->fds[fildes[1]].flags = 0;

	p->fds[fildes[0]].file = fo;
	(void)proc_lockInit(&fo->lock, &proc_lockAttrDefault, "posix.file");
	hal_memcpy(&fo->oid, &oid, sizeof(oid));
	fo->refs = 1;
	fo->offset = 0;
	fo->type = ftPipe;
	fo->status = O_RDONLY;

	p->fds[fildes[1]].file = fi;
	(void)proc_lockInit(&fi->lock, &proc_lockAttrDefault, "posix.file");
	hal_memcpy(&fi->oid, &oid, sizeof(oid));
	fi->refs = 1;
	fi->offset = 0;
	fi->type = ftPipe;
	fi->status = O_WRONLY;

	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return 0;
}


int posix_mkfifo(const char *pathname, mode_t mode)
{
	TRACE("mkfifo(%s, %x)", pathname, mode);

	oid_t oid, file;
	oid_t pipesrv;
	int ret;

	hal_memset(&oid, 0, sizeof(oid));

	if (proc_lookup("/dev/posix/pipes", NULL, &pipesrv) < 0) {
		return -ENOSYS;
	}

	ret = proc_create(pipesrv.port, pxBufferedPipe, 0U, oid, pipesrv, NULL, &oid);
	if (ret < 0) {
		return ret;
	}

	/* link pipe in posix server */
	ret = proc_link(oid, oid, pathname);
	if (ret < 0) {
		return ret;
	}

	/* create pipe in filesystem */
	ret = posix_create(pathname, 2 /* otDev */, mode | S_IFIFO, oid, &file);
	if (ret < 0) {
		return ret;
	}

	return 0;
}


int posix_chmod(const char *pathname, mode_t mode)
{
	TRACE("chmod(%s, %x)", pathname, mode);

	oid_t oid;
	msg_t msg;
	int err;

	if (proc_lookup(pathname, &oid, NULL) < 0) {
		return -ENOENT;
	}

	hal_memset(&msg, 0, sizeof(msg));
	hal_memcpy(&msg.oid, &oid, sizeof(oid));

	msg.type = mtSetAttr;
	msg.i.attr.type = atMode;
	/* parasoft-suppress-next-line MISRAC2012-RULE_10_3-b */
	msg.i.attr.val = mode & ALLPERMS;

	err = proc_send(oid.port, &msg);
	if (err >= 0) {
		err = msg.o.err;
	}

	return (err < 0) ? err : EOK;
}


int posix_link(const char *path1, const char *path2)
{
	TRACE("link(%s, %s)", path1, path2);

	oid_t oid, dev, dir;
	int err;
	char *name, *basename;
	const char *dirname;

	name = lib_strdup(path2);
	if (name == NULL) {
		return -ENOMEM;
	}

	(void)lib_splitname(name, &basename, &dirname);

	do {
		err = proc_lookup(dirname, NULL, &dir);
		if (err < 0) {
			break;
		}

		err = proc_lookup(path1, &oid, &dev);
		if (err < 0) {
			break;
		}

		if (oid.port != dir.port) {
			err = -EXDEV;
			break;
		}

		err = proc_link(dir, oid, basename);
		if (err < 0) {
			break;
		}
		if (dev.port != oid.port) {
			/* Signal link to device */
			/* FIXME: refcount here? */
			err = proc_link(dev, dev, path2);
			if (err < 0) {
				break;
			}
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
	char *name, *basename;
	const char *dirname;

	name = lib_strdup(pathname);
	if (name == NULL) {
		return -ENOMEM;
	}

	(void)lib_splitname(name, &basename, &dirname);

	do {
		err = proc_lookup(dirname, NULL, &dir);
		if (err < 0) {
			break;
		}

		err = proc_lookup(pathname, NULL, &oid);
		if (err < 0) {
			break;
		}

		err = proc_unlink(dir, oid, basename);
		if (err < 0) {
			break;
		}

		if (dir.port != oid.port) {
			if (oid.port == US_PORT) {
				(void)unix_unlink(oid.id);
			}
			else {
				/* Signal unlink to device */
				/* FIXME: refcount here? */
				err = proc_unlink(oid, oid, pathname);
				if (err < 0) {
					break;
				}
			}
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	return err;
}


int posix_lseek(int fildes, off_t *offset, int whence)
{
	TRACE("seek(%d, %d, %d)", fildes, offset, whence);

	open_file_t *f;
	off_t scnt;
	int err = 0;

	err = posix_getOpenFile(fildes, &f);
	if (err != 0) {
		return err;
	}

	/* TODO: Find a better way to check fd type */
	scnt = proc_size(f->oid);
	if (scnt < 0) {
		(void)posix_fileDeref(f);
		return -ESPIPE;
	}

	(void)proc_lockSet(&f->lock);
	switch (whence) {
		case SEEK_SET:
			scnt = *offset;
			break;

		case SEEK_CUR:
			scnt = f->offset + *offset;
			break;

		case SEEK_END:
			scnt += *offset;
			break;

		default:
			scnt = -1;
			break;
	}

	if (scnt >= 0) {
		f->offset = scnt;
	}
	else {
		err = -EINVAL;
	}

	(void)proc_lockClear(&f->lock);

	(void)posix_fileDeref(f);

	*offset = scnt;

	return err;
}


int posix_ftruncate(int fildes, off_t length)
{
	TRACE("ftruncate(%d)", fildes);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(fildes, &f);
	if (err >= 0) {
		if ((f->status & O_RDONLY) == 0U) {
			err = posix_truncate(&f->oid, length);
		}
		else {
			err = -EBADF;
		}
		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_fstat(int fd, struct stat *buf)
{
	TRACE("fstat(%d)", fd);

	open_file_t *f;
	msg_t msg;
	int err;
	struct _attrAll attrs;

	err = posix_getOpenFile(fd, &f);
	if (err < 0) {
		return err;
	}

	hal_memset(buf, 0, sizeof(struct stat));
	hal_memset(&msg, 0, sizeof(msg_t));

	buf->st_dev = (int)f->ln.port;
	buf->st_ino = f->ln.id; /* FIXME */
	buf->st_rdev = (int)f->oid.port;

	if (f->type == ftRegular) {
		msg.type = mtGetAttrAll;
		hal_memcpy(&msg.oid, &f->oid, sizeof(oid_t));
		msg.o.data = &attrs;
		msg.o.size = sizeof(attrs);

		do {
			err = proc_send(f->oid.port, &msg);
			if (err < 0) {
				break;
			}

			err = msg.o.err;
			if (err < 0) {
				break;
			}

			err = attrs.mTime.err;
			if (err < 0) {
				break;
			}
			buf->st_mtim.tv_sec = attrs.mTime.val;
			buf->st_mtim.tv_nsec = 0;

			err = attrs.aTime.err;
			if (err < 0) {
				break;
			}

			buf->st_atim.tv_sec = attrs.aTime.val;
			buf->st_atim.tv_nsec = 0;

			err = attrs.cTime.err;
			if (err < 0) {
				break;
			}
			buf->st_ctim.tv_sec = attrs.cTime.val;
			buf->st_ctim.tv_nsec = 0;

			err = attrs.links.err;
			if (err < 0) {
				break;
			}
			buf->st_nlink = (int)attrs.links.val;

			err = attrs.mode.err;
			if (err < 0) {
				break;
			}
			buf->st_mode = (unsigned int)attrs.mode.val;

			err = attrs.uid.err;
			if (err < 0) {
				break;
			}
			buf->st_uid = (int)attrs.uid.val;

			err = attrs.gid.err;
			if (err < 0) {
				break;
			}
			buf->st_gid = (int)attrs.gid.val;

			err = attrs.size.err;
			if (err < 0) {
				break;
			}
			buf->st_size = attrs.size.val;

			err = attrs.blocks.err;
			if (err < 0) {
				break;
			}
			buf->st_blocks = attrs.blocks.val;

			err = attrs.ioblock.err;
			if (err < 0) {
				break;
			}
			buf->st_blksize = (int)attrs.ioblock.val;
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

	(void)posix_fileDeref(f);

	return err;
}


int posix_fsync(int fd)
{
	TRACE("fsync(%d)", fd);

	open_file_t *f;
	msg_t msg;
	int err;

	err = posix_getOpenFile(fd, &f);
	if (err < 0) {
		return err;
	}

	hal_memset(&msg, 0, sizeof(msg_t));

	/* FIXME: Replace this hack, pass oid via msg_t root struct */
	msg.type = 0xf52; /* mtSync */

	hal_memcpy(msg.i.raw, &f->oid, sizeof(f->oid));

	err = proc_send(f->oid.port, &msg);

	(void)posix_fileDeref(f);

	return err;
}


static int posix_fcntlDup(int fd, int fd2, int cloexec)
{
	process_info_t *p;
	int err;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	(void)proc_lockSet(&p->lock);
	if ((fd < 0) || (fd >= p->fdsz) || (fd2 < 0) || (fd2 >= p->maxfd)) {
		(void)proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	fd2 = _posix_allocfd(p, fd2);
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_7-a "fd2 value is checked within posix_dup2" */
	err = _posix_dup2(p, fd, fd2);
	if ((err == fd2) && (cloexec != 0)) {
		p->fds[fd2].flags = FD_CLOEXEC;
	}

	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


static int posix_fcntlSetFd(int fd, int flags)
{
	process_info_t *p;
	int err = EOK;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -ENOSYS;
	}

	(void)proc_lockSet(&p->lock);
	if ((fd < 0) || (fd >= p->fdsz)) {
		(void)proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	if (p->fds[fd].file != NULL) {
		p->fds[fd].flags = (unsigned int)flags;
	}
	else {
		err = -EBADF;
	}
	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


static int posix_fcntlGetFd(int fd)
{
	process_info_t *p;
	int err;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -ENOSYS;
	}

	(void)proc_lockSet(&p->lock);
	if ((fd < 0) || (fd >= p->fdsz)) {
		(void)proc_lockClear(&p->lock);
		pinfo_put(p);
		return -EBADF;
	}

	if (p->fds[fd].file != NULL) {
		err = (int)p->fds[fd].flags;
	}
	else {
		err = -EBADF;
	}
	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return err;
}


static int posix_fcntlSetFl(int fd, int val)
{
	open_file_t *f;
	int err;
	/* Creation and access mode flags shall be ignored */
	unsigned int ignorefl = O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_RDONLY | O_RDWR | O_WRONLY;

	err = posix_getOpenFile(fd, &f);
	if (err == 0) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_setfl(f->oid.port, val);
				break;
			case ftUnixSocket:
				err = unix_setfl(f->oid.id, (unsigned int)val);
				break;
			default:
				f->status = ((unsigned int)val & ~ignorefl) | (f->status & ignorefl);
				break;
		}

		(void)posix_fileDeref(f);
	}

	return err;
}


static int posix_fcntlGetFl(int fd)
{
	open_file_t *f;
	int err;

	err = posix_getOpenFile(fd, &f);
	if (err == 0) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_getfl(f->oid.port);
				break;
			case ftUnixSocket:
				err = unix_getfl(f->oid.id);
				break;
			default:
				err = (int)f->status;
				break;
		}

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_fcntl(int fd, unsigned int cmd, u8 *ustack)
{
	TRACE("fcntl(%d, %u)", fd, cmd);

	int err = -EINVAL, fd2;
	unsigned long arg;
	int cloexec = 0;

	switch (cmd) {
		case F_DUPFD_CLOEXEC:
		case F_DUPFD:
			GETFROMSTACK(ustack, int, fd2, 2);
			err = posix_fcntlDup(fd, fd2, cloexec);
			break;

		case F_GETFD:
			err = posix_fcntlGetFd(fd);
			break;

		case F_SETFD:
			GETFROMSTACK(ustack, unsigned long, arg, 2);
			err = posix_fcntlSetFd(fd, (int)arg);
			break;

		case F_GETFL:
			err = posix_fcntlGetFl(fd);
			break;

		case F_SETFL:
			GETFROMSTACK(ustack, unsigned int, arg, 2);
			err = posix_fcntlSetFl(fd, (int)arg);
			break;

		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			/* TODO: implement */
			err = EOK;
			break;
		case F_GETOWN:
		case F_SETOWN:
		default:
			/* Handles any value of 'cmd' not covered by the case labels. */
			break;
	}

	return err;
}


#define IOCPARM_MASK   0x1fffUL
#define IOCPARM_LEN(x) (((x) >> 16) & IOCPARM_MASK)

#define IOC_OUT                      0x40000000UL
#define IOC_IN                       0x80000000UL
#define IOC_INOUT                    (IOC_IN | IOC_OUT)
#define _IOC(inout, group, num, len) ((unsigned long)((inout) | (((len) & IOCPARM_MASK) << 16) | (((unsigned int)(group)) << 8) | (num)))

#define SIOCGIFCONF _IOC(IOC_INOUT, 'S', 0x12U, sizeof(struct ifconf))
#define SIOCADDRT   _IOC(IOC_IN, 'S', 0x44U, sizeof(struct rtentry))
#define SIOCDELRT   _IOC(IOC_IN, 'S', 0x45U, sizeof(struct rtentry))


static void ioctl_pack(msg_t *msg, unsigned long request, void *data, oid_t *oid)
{
	size_t size = IOCPARM_LEN(request);
	ioctl_in_t *ioctl = (ioctl_in_t *)msg->i.raw;
	struct ifconf *ifc;
	struct rtentry *rt;

	hal_memcpy(&msg->oid, oid, sizeof(*oid));
	msg->type = mtDevCtl;
	msg->i.data = NULL;
	msg->i.size = 0;
	msg->o.data = NULL;
	msg->o.size = 0;

	ioctl->request = request;

	if ((request & IOC_INOUT) != 0U) {
		if ((request & IOC_IN) != 0U) {
			if (size <= (sizeof(msg->i.raw) - sizeof(ioctl_in_t))) {
				hal_memcpy(ioctl->data, data, size);
			}
			else {
				msg->i.data = data;
				msg->i.size = size;
			}
		}

		if (((request & IOC_OUT) != 0U) && (size > sizeof(msg->o.raw))) {
			msg->o.data = data;
			msg->o.size = size;
		}
	}
	else if (size > 0U) {
		/* the data is passed by value instead of pointer */
		size = min(size, sizeof(void *));
		hal_memcpy(ioctl->data, &data, size);
	}
	else {
		/* No action required */
	}


	/* ioctl special case: arg is structure with pointer - has to be custom-packed into message */
	if (request == SIOCGIFCONF) {
		ifc = (struct ifconf *)data;
		msg->o.data = ifc->ifc_buf;
		msg->o.size = ifc->ifc_len;
	}
	else if ((request == SIOCADDRT) || (request == SIOCDELRT)) {
		rt = (struct rtentry *)data;
		if (rt->rt_dev != NULL) {
			msg->o.data = rt->rt_dev;
			msg->o.size = hal_strlen(rt->rt_dev) + 1U;
		}
	}
	else {
		/* No action required */
	}
}


static int ioctl_processResponse(const msg_t *msg, unsigned long request, void *data)
{
	size_t size = IOCPARM_LEN(request);
	int err;
	struct ifconf *ifc;

	err = msg->o.err;

	if (((request & IOC_OUT) != 0U) && (size <= sizeof(msg->o.raw))) {
		hal_memcpy(data, msg->o.raw, size);
	}

	if (request == SIOCGIFCONF) { /* restore overridden userspace pointer */
		ifc = (struct ifconf *)data;
		ifc->ifc_buf = msg->o.data;
	}

	return err;
}


int posix_ioctl(int fildes, unsigned long request, u8 *ustack)
{
	TRACE("ioctl(%d, %d)", fildes, request);

	open_file_t *f;
	int err;
	msg_t msg;
	void *data = NULL;

	err = posix_getOpenFile(fildes, &f);
	if (err == 0) {
		/* TODO: handle POSIX defined requests with `switch (request)` */
		if (((request & IOC_INOUT) != 0U) || (IOCPARM_LEN(request) > 0U)) {
			GETFROMSTACK(ustack, void *, data, 2);
		}

		ioctl_pack(&msg, request, data, &f->oid);

		err = proc_send(f->oid.port, &msg);
		if (err == EOK) {
			err = ioctl_processResponse(&msg, request, data);
		}

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_socket(int domain, int type, int protocol)
{
	TRACE("socket(%d, %d, %d)", domain, type, protocol);

	process_info_t *p;
	int err, fd;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	fd = posix_newFile(p, 0);
	if (fd < 0) {
		pinfo_put(p);
		return -EMFILE;
	}

	switch (domain) {
		case AF_UNIX:
			err = unix_socket(domain, type, protocol);
			if (err >= 0) {
				p->fds[fd].file->type = ftUnixSocket;
				p->fds[fd].file->oid.port = US_PORT;
				p->fds[fd].file->oid.id = (unsigned int)err;
			}
			break;
		case AF_INET:
		case AF_INET6:
		case AF_KEY:
		case AF_PACKET:
			err = inet_socket(domain, type, protocol);
			if (err >= 0) {
				p->fds[fd].file->type = ftInetSocket;
				p->fds[fd].file->oid.port = (unsigned int)err;
				p->fds[fd].file->oid.id = 0U;
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

	if (((unsigned int)type & SOCK_CLOEXEC) != 0U) {
		p->fds[fd].flags = FD_CLOEXEC;
	}

	pinfo_put(p);
	return fd;
}


int posix_socketpair(int domain, int type, int protocol, int sv[2])
{
	TRACE("socketpair(%d, %d, %d, %p)", domain, type, protocol, sv);

	process_info_t *p;
	int err, id[2];

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	if (domain != AF_UNIX) {
		return -EAFNOSUPPORT;
	}

	sv[0] = posix_newFile(p, 0);
	if (sv[0] < 0) {
		pinfo_put(p);
		return -EMFILE;
	}

	sv[1] = posix_newFile(p, 0);
	if (sv[1] < 0) {
		posix_putUnusedFile(p, sv[0]);
		pinfo_put(p);
		return -EMFILE;
	}

	err = unix_socketpair(domain, type, protocol, id);
	if (err == 0) {
		p->fds[sv[0]].file->type = ftUnixSocket;
		p->fds[sv[1]].file->type = ftUnixSocket;
		p->fds[sv[0]].file->oid.port = US_PORT;
		p->fds[sv[1]].file->oid.port = US_PORT;
		p->fds[sv[0]].file->oid.id = (id_t)id[0];
		p->fds[sv[1]].file->oid.id = (id_t)id[1];

		if (((unsigned int)type & SOCK_CLOEXEC) != 0U) {
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

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	fd = posix_newFile(p, 0);
	if (fd < 0) {
		pinfo_put(p);
		return -EMFILE;
	}

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_accept4(f->oid.port, address, address_len, (unsigned int)flags);
				if (err >= 0) {
					p->fds[fd].file->type = ftInetSocket;
					p->fds[fd].file->oid.port = (unsigned int)err;
					p->fds[fd].file->oid.id = 0;
				}
				break;
			case ftUnixSocket:
				err = unix_accept4(f->oid.id, address, address_len, (unsigned int)flags);
				if (err >= 0) {
					p->fds[fd].file->type = ftUnixSocket;
					p->fds[fd].file->oid.port = US_PORT;
					p->fds[fd].file->oid.id = (unsigned int)err;
				}
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		(void)posix_fileDeref(f);
	}

	if (err < 0) {
		posix_putUnusedFile(p, fd);
		pinfo_put(p);
		return err;
	}

	if (((unsigned int)flags & SOCK_CLOEXEC) != 0U) {
		p->fds[fd].flags = FD_CLOEXEC;
	}

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

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	TRACE("connect(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_uname(struct utsname *name)
{
	TRACE("uname()");

	(void)hal_strncpy(name->sysname, "Phoenix-RTOS", sizeof(name->sysname) - 1U);
	name->sysname[sizeof(name->sysname) - 1U] = '\0';
	(void)hal_strncpy(name->nodename, posix_common.hostname, sizeof(name->nodename) - 1U);
	name->nodename[sizeof(name->nodename) - 1U] = '\0';
	(void)hal_strncpy(name->release, RELEASE, sizeof(name->release) - 1U);
	name->release[sizeof(name->release) - 1U] = '\0';
	(void)hal_strncpy(name->version, VERSION, sizeof(name->version) - 1U);
	name->version[sizeof(name->version) - 1U] = '\0';
	(void)hal_strncpy(name->machine, TARGET_FAMILY, sizeof(name->machine) - 1U);
	name->machine[sizeof(name->machine) - 1U] = '\0';

	return 0;
}


int posix_gethostname(char *name, size_t namelen)
{
	TRACE("gethostname(%zu)", namelen);

	(void)hal_strncpy(name, posix_common.hostname, namelen);

	return 0;
}


int posix_getpeername(int socket, struct sockaddr *address, socklen_t *address_len)
{
	TRACE("getpeername(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_getsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
	TRACE("getsockname(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
	TRACE("getsockopt(%d, %d, %d)", socket, level, optname);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_listen(int socket, int backlog)
{
	TRACE("listen(%d, %d)", socket, backlog);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_recvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	TRACE("recvfrom(%d, %d, %s)", socket, length, src_addr == NULL ? NULL : src_addr->sa_data);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_recvfrom(f->oid.port, message, length, (unsigned int)flags, src_addr, src_len);
				break;
			case ftUnixSocket:
				err = unix_recvfrom(f->oid.id, message, length, (unsigned int)flags, src_addr, src_len);
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		(void)posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	TRACE("sendto(%d, %s, %d, %s)", socket, message, length, dest_addr == NULL ? NULL : dest_addr->sa_data);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_sendto(f->oid.port, message, length, (unsigned int)flags, dest_addr, dest_len);
				break;
			case ftUnixSocket:
				err = unix_sendto(f->oid.id, message, length, (unsigned int)flags, dest_addr, dest_len);
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		(void)posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_recvmsg(int socket, struct msghdr *msg, int flags)
{
	TRACE("recvmsg(%d, %p, %d)", socket, msg, flags);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_recvmsg(f->oid.port, msg, (unsigned int)flags);
				break;
			case ftUnixSocket:
				err = unix_recvmsg(f->oid.id, msg, (unsigned int)flags);
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		(void)posix_fileDeref(f);
	}

	return err;
}


ssize_t posix_sendmsg(int socket, const struct msghdr *msg, int flags)
{
	TRACE("sendmsg(%d, %p, %d)", socket, msg, flags);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
		switch (f->type) {
			case ftInetSocket:
				err = inet_sendmsg(f->oid.port, msg, (unsigned int)flags);
				break;
			case ftUnixSocket:
				err = unix_sendmsg(f->oid.id, msg, (unsigned int)flags);
				break;
			default:
				err = -ENOTSOCK;
				break;
		}

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_shutdown(int socket, int how)
{
	TRACE("shutdown(%d, %d)", socket, how);

	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
	}

	return err;
}


int posix_sethostname(const char *name, size_t namelen)
{
	TRACE("sethostname(%zu)", namelen);

	if (namelen > HOST_NAME_MAX) {
		return -EINVAL;
	}

	(void)hal_strncpy(posix_common.hostname, name, namelen);
	posix_common.hostname[namelen] = '\0';

	return 0;
}


int posix_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	open_file_t *f;
	int err;

	err = posix_getOpenFile(socket, &f);
	if (err == 0) {
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

		(void)posix_fileDeref(f);
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
	if (err < 0) {
		return err;
	}

	hal_memset(&msg, 0, sizeof(msg_t));

	msg.type = mtSetAttr;
	hal_memcpy(&msg.oid, &f->oid, sizeof(oid_t));

	msg.i.attr.type = atMTime;
	msg.i.attr.val = (long long)times[1].tv_sec;
	err = proc_send(f->oid.port, &msg);
	if ((err >= 0) && (msg.o.err >= 0)) {
		msg.i.attr.type = atATime;
		msg.i.attr.val = (long long)times[0].tv_sec;
		err = proc_send(f->oid.port, &msg);
	}
	if (err >= 0) {
		err = msg.o.err;
	}

	(void)posix_fileDeref(f);

	return err;
}


static int do_poll_iteration(struct pollfd *fds, nfds_t nfds)
{
	msg_t msg;
	int ready = 0, i;
	int err;
	open_file_t *f;

	hal_memset(&msg, 0, sizeof(msg));

	msg.type = mtGetAttr;
	msg.i.attr.type = atPollStatus;

	for (i = 0; i < (int)nfds; ++i) {
		if (fds[i].fd < 0) {
			continue;
		}

		msg.i.attr.val = (long long)fds[i].events;

		if (posix_getOpenFile(fds[i].fd, &f) < 0) {
			err = (int)POLLNVAL;
		}
		else {
			hal_memcpy(&msg.oid, &f->oid, sizeof(oid_t));
			(void)posix_fileDeref(f);

			if (f->type == ftUnixSocket) {
				err = unix_poll((unsigned int)msg.oid.id, (unsigned short)fds[i].events);
			}
			else {
				err = proc_send(msg.oid.port, &msg);
				if (err >= 0) {
					/* FIXME: 8 byte attr assigned to 4 byte err */
					err = (msg.o.err >= 0) ? (int)msg.o.attr.val : msg.o.err;
				}
			}
		}

		if (err == -EINTR) {
			return err;
		}

		if (err < 0) {
			fds[i].revents = (short)(unsigned short)((unsigned short)fds[i].revents | POLLHUP);
		}
		else if (err > 0) {
			fds[i].revents = (short)(unsigned short)((unsigned short)fds[i].revents | (unsigned short)err);
		}
		else {
			/* No action required */
		}

		fds[i].revents = (short)(unsigned short)((unsigned short)fds[i].revents & ~(~(unsigned short)fds[i].events & (POLLIN | POLLOUT | POLLPRI | POLLRDNORM | POLLWRNORM | POLLRDBAND | POLLWRBAND)));

		if (fds[i].revents != 0) {
			++ready;
		}
	}

	return ready;
}

#if 1
int posix_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms)
{
	unsigned int i, n;
	int ready;
	time_t timeout, now;

	n = 0U;

	for (i = 0U; i < nfds; ++i) {
		fds[i].revents = 0;
		if (fds[i].fd >= 0) {
			++n;
		}
	}

	if (n == 0U) {
		if (timeout_ms > 0) {
			(void)proc_threadSleep(timeout_ms * 1000LL);
		}
		return 0;
	}

	if (timeout_ms >= 0) {
		proc_gettime(&timeout, NULL);
		timeout += timeout_ms * 1000LL;
		timeout += (timeout_ms == 0L) ? 1LL : 0LL;
	}
	else {
		timeout = 0;
	}

	ready = do_poll_iteration(fds, nfds);
	while (ready == 0) {
		if (timeout != 0) {
			proc_gettime(&now, NULL);
			if (now > timeout) {
				break;
			}

			now = timeout - now;
			if (now > POLL_INTERVAL) {
				now = POLL_INTERVAL;
			}
		}
		else {
			now = POLL_INTERVAL;
		}

		(void)proc_threadSleep(now);
		ready = do_poll_iteration(fds, nfds);
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
	err = do_poll_iteration(fds, nfds);
	if (err != 0) {
		return err;
	}
	else if (timeout_ms == 0) {
		return 0;
	}
	else {
		/* MISRA */
	}

	queue = posix_open("/dev/event/queue", O_RDWR, NULL);
	if (queue < 0) {
		return queue;
	}

	do {
		if (posix_getOpenFile(queue, &q) < 0)
			return -EAGAIN; /* should not happen? */

		if (nfds > sizeof(subs_stack) / sizeof(evsub_t)) {
			return -ENOMEM;
		}

		subs = vm_kmalloc(nfds * sizeof(evsub_t));
		if (subs == NULL) {
			return -ENOMEM;
		}

		hal_memset(subs, 0, nfds * sizeof(evsub_t));

		do {
			err = EOK;

			for (i = 0; i < nfds; ++i) {
				if (fds[i].fd < 0)
					continue;

				err = posix_getOpenFile(fds[i].fd, &f);
				if (err != 0) {
					fds[i].revents = POLLNVAL;
					continue;
				}

				hal_memcpy(&subs[i].oid, &f->oid, sizeof(oid_t));
				subs[i].flags = evAdd;
				subs[i].types = fds[i].events;
			}

			if (err != 0) {
				break;
			}

			msg.type = mtRead;

			hal_memcpy(&msg.i.io.oid, &q->oid, sizeof(oid_t));
			msg.i.io.len = (unsigned int)timeout_ms;

			msg.i.data = subs;
			msg.i.size = nfds * sizeof(evsub_t);

			msg.o.data = events;
			msg.o.size = sizeof(events);

			err = proc_send(q->oid.port, &msg);
			if (err != 0) {
				break;
			}

			err = msg.o.io.err;
			if (err < 0) {
				break;
			}

			if (err == 0) {
				break;
			}

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

	pinfo = pinfo_find(pid);
	if (pinfo == NULL) {
		return -ESRCH;
	}

	proc = proc_find(pinfo->process);
	if (proc == NULL) {
		pinfo_put(pinfo);
		return -ESRCH;
	}

	if (tid == 0) {
		err = threads_sigpost(proc, NULL, sig);
	}
	else {
		thr = threads_findThread(tid);
		if (thr == NULL) {
			(void)proc_put(proc);
			pinfo_put(pinfo);
			return -EINVAL;
		}

		if (thr->process == proc) {
			err = threads_sigpost(proc, thr, sig);
		}
		else {
			err = -EINVAL;
		}

		threads_put(thr);
	}
	(void)proc_put(proc);
	pinfo_put(pinfo);

	return err;
}


static int posix_killGroup(pid_t pgid, int sig)
{
	process_info_t *pinfo;
	rbnode_t *node;

	(void)proc_lockSet(&posix_common.lock);
	for (node = lib_rbMinimum(posix_common.pid.root); node != NULL; node = lib_rbNext(node)) {
		pinfo = lib_treeof(process_info_t, linkage, node);

		if (pinfo->pgid == pgid) {
			(void)proc_sigpost(pinfo->process, sig);
		}
	}
	(void)proc_lockClear(&posix_common.lock);

	return EOK;
}


int posix_tkill(pid_t pid, int tid, int sig)
{
	TRACE("tkill(%p, %d, %d)", pid, tid, sig);

	if ((sig < 0) || (sig > NSIG)) {
		return -EINVAL;
	}

	/* TODO: handle pid = 0 */
	if (pid == 0) {
		return -ENOSYS;
	}

	if (pid == -1) {
		return -ESRCH;
	}

	return (pid > 0) ? posix_killOne(pid, tid, sig) : posix_killGroup(-pid, sig);
}


void posix_sigchild(pid_t ppid)
{
	(void)posix_tkill(ppid, 0, SIGCHLD);
}


int posix_setpgid(pid_t pid, pid_t pgid)
{
	process_info_t *pinfo;

	if ((pid < 0) || (pgid < 0)) {
		return -EINVAL;
	}

	if (pid == 0) {
		pid = process_getPid(proc_current()->process);
	}

	if (pgid == 0) {
		pgid = pid;
	}

	pinfo = pinfo_find(pid);
	if (pinfo == NULL) {
		return -ESRCH;
	}

	(void)proc_lockSet(&pinfo->lock);
	pinfo->pgid = pgid;
	(void)proc_lockClear(&pinfo->lock);
	pinfo_put(pinfo);
	return EOK;
}


pid_t posix_getpgid(pid_t pid)
{
	process_info_t *pinfo;
	pid_t res;

	if (pid < 0) {
		return -EINVAL;
	}

	if (pid == 0) {
		pid = process_getPid(proc_current()->process);
	}

	pinfo = pinfo_find(pid);
	if (pinfo == NULL) {
		return -ESRCH;
	}

	(void)proc_lockSet(&pinfo->lock);
	res = pinfo->pgid;
	(void)proc_lockClear(&pinfo->lock);
	pinfo_put(pinfo);

	return res;
}


pid_t posix_setsid(void)
{
	process_info_t *pinfo;
	pid_t pid;

	pid = process_getPid(proc_current()->process);

	pinfo = pinfo_find(pid);
	if (pinfo == NULL) {
		return -EPERM;
	}

	/* FIXME (pedantic): Should check if any process has my group id */
	(void)proc_lockSet(&pinfo->lock);
	if (pinfo->pgid == pid) {
		(void)proc_lockClear(&pinfo->lock);
		pinfo_put(pinfo);
		return -EPERM;
	}

	pinfo->pgid = pid;
	(void)proc_lockClear(&pinfo->lock);
	pinfo_put(pinfo);

	return pid;
}


int posix_waitpid(pid_t child, int *status, unsigned int options)
{
	process_info_t *pinfo, *c;
	pid_t pid;
	int err = EOK;

	pid = process_getPid(proc_current()->process);

	pinfo = pinfo_find(pid);
	LIB_ASSERT_ALWAYS(pinfo != NULL, "pinfo not found, pid: %d", pid);

	(void)proc_lockSet(&pinfo->lock);
	for (;;) {
		/* Do this in the loop in case someone has a bad idea of doing multithreaded waitpid */
		if ((pinfo->children == NULL) && (pinfo->zombies == NULL)) {
			err = -ECHILD;
			break;
		}

		if (pinfo->zombies != NULL) {
			c = pinfo->zombies;
			do {
				if ((child == -1) || ((child == 0) && (c->pgid == pinfo->pgid)) ||
						((child < 0) && (c->pgid == -child)) || (child == c->process)) {
					LIST_REMOVE(&pinfo->zombies, c);
					err = c->process;
					if (status != NULL) {
						*status = c->exitcode;
					}
					(void)proc_lockClear(&pinfo->lock);

					pinfo_put(c);
					pinfo_put(pinfo);
					return err;
				}

				c = c->next;
			} while (c != pinfo->zombies);
		}

		if ((options & 1U) != 0U) { /* WNOHANG */
			err = EOK;
			break;
		}

		do {
			err = proc_lockWait(&pinfo->wait, &pinfo->lock, 0);
		} while ((pinfo->zombies == NULL) && (err == EOK));

		if (err == -EINTR) {
			/* pinfo->lock is clear */
			pinfo_put(pinfo);
			return -EINTR;
		}
		else if (err != 0) {
			/* Should not happen */
			break;
		}
		else {
			/* No action required */
		}
	}
	(void)proc_lockClear(&pinfo->lock);
	pinfo_put(pinfo);

	return err;
}


void posix_died(pid_t pid, int exit)
{
	process_info_t *pinfo, *ppinfo, *init, *cinfo, *zinfo, *zombies;
	int waited, adopted = 1;

	pinfo = pinfo_find(pid);
	LIB_ASSERT_ALWAYS(pinfo != NULL, "pinfo not found, pid: %d", pid);

	init = pinfo_find(1);
	LIB_ASSERT_ALWAYS(init != NULL, "init not found");

	ppinfo = pinfo_find(pinfo->parent);

	(void)posix_exit(pinfo, exit);

	/* We might not find a parent if it died just now */
	if (ppinfo != NULL) {
		/* Make a zombie, wakeup waitpid */
		(void)proc_lockSet(&ppinfo->lock);
		/* Check if we didn't get adopted by the init in the meantime */
		if ((ppinfo != init) && (LIST_BELONGS(&ppinfo->children, pinfo) != 0)) {
			LIST_REMOVE(&ppinfo->children, pinfo);
			LIST_ADD(&ppinfo->zombies, pinfo);
			waited = proc_threadBroadcast(&ppinfo->wait);
			adopted = 0;
		}
		(void)proc_lockClear(&ppinfo->lock);
		pinfo_put(ppinfo);
	}

	(void)proc_lockSet2(&pinfo->lock, &init->lock);
	/* Collect all zombies */
	zombies = pinfo->zombies;
	pinfo->zombies = NULL;

	/* Adopt children */
	while (pinfo->children != NULL) {
		cinfo = pinfo->children;
		LIST_REMOVE(&pinfo->children, cinfo);
		/* Treat as atomic */
		cinfo->parent = 1;
		LIST_ADD(&init->children, cinfo);
	}

	if (adopted != 0) {
		LIB_ASSERT(LIST_BELONGS(&init->children, pinfo) != 0,
				"zombie's neither parent nor init child, pid: %d, ppid: %d", pid, pinfo->parent);
		/* We were adopted by the init at some point */
		LIST_REMOVE(&init->children, pinfo);
		LIST_ADD(&zombies, pinfo);
		waited = 1;
	}
	(void)proc_lockClear(&pinfo->lock);
	(void)proc_lockClear(&init->lock);
	pinfo_put(init);

	/* Reap all orphaned zombies */
	while (zombies != NULL) {
		zinfo = zombies;
		LIST_REMOVE(&zombies, zinfo);
		pinfo_put(zinfo);
	}

	/* Signal parent if no one was waiting in waitpid() */
	if (waited == 0) {
		posix_sigchild(pinfo->parent);
	}

	pinfo_put(pinfo);
}


pid_t posix_getppid(pid_t pid)
{
	process_info_t *pinfo;
	int ret = 0;

	pinfo = pinfo_find(pid);
	if (pinfo == NULL) {
		return -ENOSYS;
	}

	ret = pinfo->parent;

	pinfo_put(pinfo);

	return ret;
}


void posix_init(void)
{
	(void)proc_lockInit(&posix_common.lock, &proc_lockAttrDefault, "posix.common");
	lib_rbInit(&posix_common.pid, pinfo_cmp, NULL);
	unix_sockets_init();
	posix_common.fresh = 0;
	hal_memset(posix_common.hostname, 0, sizeof(posix_common.hostname));
}
