/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * File descriptor passing
 *
 * Copyright 2021 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "fdpass.h"


#define CMSG_ALIGN(n)             (((n) + sizeof(socklen_t) - 1U) & ~(sizeof(socklen_t) - 1U))
#define CMSG_SPACE(n)             (sizeof(struct cmsghdr) + CMSG_ALIGN(n))
#define CMSG_LEN(n)               (sizeof(struct cmsghdr) + (n))
#define CMSG_DATA(c)              ((unsigned char *)((struct cmsghdr *)(c) + 1))
#define CMSG_DATA_CONST(c)        ((const unsigned char *)((const struct cmsghdr *)(c) + 1))
#define CMSG_FIRSTHDR(d, l)       ((l) < sizeof(struct cmsghdr) ? NULL : (struct cmsghdr *)(d))
#define CMSG_FIRSTHDR_CONST(d, l) ((l) < sizeof(struct cmsghdr) ? NULL : (const struct cmsghdr *)(d))

#define CMSG_NXTHDR(d, l, c) \
	({ \
		struct cmsghdr *_c = (struct cmsghdr *)(c); \
		char *_n = (char *)_c + CMSG_SPACE(_c->cmsg_len); \
		char *_e = (char *)(d) + (l); \
		((ptr_t)_n > (ptr_t)_e ? (struct cmsghdr *)NULL : (struct cmsghdr *)_n); \
	})

#define CMSG_NXTHDR_CONST(d, l, c) \
	({ \
		const char *_n = (const char *)(c) + CMSG_SPACE((c)->cmsg_len); \
		const char *_e = (const char *)(d) + (l); \
		((ptr_t)_n > (ptr_t)_e ? NULL : (const struct cmsghdr *)_n); \
	})


#define FDPACK_PUSH(p, of, fl) \
	do { \
		(p)->fd[(p)->first + (p)->cnt].file = (of); \
		(p)->fd[(p)->first + (p)->cnt].flags = (fl); \
		++(p)->cnt; \
	} while (0)

#define FDPACK_POP_FILE(p, of) \
	do { \
		(of) = (p)->fd[(p)->first].file; \
		++(p)->first; \
		--(p)->cnt; \
	} while (0)

#define FDPACK_POP_FILE_AND_FLAGS(p, of, fl) \
	do { \
		(of) = (p)->fd[(p)->first].file; \
		(fl) = (p)->fd[(p)->first].flags; \
		++(p)->first; \
		--(p)->cnt; \
	} while (0)


int fdpass_pack(fdpack_t **packs, const void *control, socklen_t controllen)
{
	fdpack_t *pack;
	const struct cmsghdr *cmsg;
	const unsigned char *cmsg_data, *cmsg_end;
	open_file_t *file;
	unsigned int cnt, tot_cnt;
	int fd, err;

	if (controllen > MAX_MSG_CONTROLLEN) {
		return -ENOMEM;
	}

	tot_cnt = 0U;
	/* calculate total number of file descriptors */
	for (cmsg = CMSG_FIRSTHDR_CONST(control, controllen); cmsg != NULL; cmsg = CMSG_NXTHDR_CONST(control, controllen, cmsg)) {
		cmsg_data = CMSG_DATA_CONST(cmsg);
		cmsg_end = (const unsigned char *)cmsg + cmsg->cmsg_len;
		cnt = (unsigned int)(((addr_t)cmsg_end - (addr_t)cmsg_data) / sizeof(int));

		if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
			return -EINVAL;
		}

		tot_cnt += cnt;
	}

	if (tot_cnt == 0U) {
		/* control data valid but no file descriptors */
		*packs = NULL;
		return 0;
	}

	pack = vm_kmalloc(sizeof(fdpack_t) + sizeof(fildes_t) * tot_cnt);
	if (pack == NULL) {
		return -ENOMEM;
	}

	hal_memset(pack, 0, sizeof(fdpack_t));

	LIST_ADD(packs, pack);

	/* reference and pack file descriptors */
	for (cmsg = CMSG_FIRSTHDR_CONST(control, controllen); cmsg != NULL; cmsg = CMSG_NXTHDR_CONST(control, controllen, cmsg)) {
		cmsg_data = CMSG_DATA_CONST(cmsg);
		cmsg_end = (const unsigned char *)cmsg + cmsg->cmsg_len;
		cnt = (unsigned int)(((addr_t)cmsg_end - (addr_t)cmsg_data) / sizeof(int));

		while (cnt != 0U) {
			hal_memcpy(&fd, cmsg_data, sizeof(int));

			err = posix_getOpenFile(fd, &file);
			if (err < 0) {
				/* revert everything we have done so far */
				(void)fdpass_discard(packs);
				return err;
			}

			/* file descriptor flags are not shared with the receiver */
			FDPACK_PUSH(pack, file, 0);

			cmsg_data += sizeof(int);
			--cnt;
		}
	}

	return 0;
}


int fdpass_unpack(fdpack_t **packs, void *control, socklen_t *controllen, unsigned int peek)
{
	process_info_t *p;
	fdpack_t *pack, *removed = NULL;
	struct cmsghdr *cmsg;
	unsigned char *cmsg_data;
	open_file_t *file;
	unsigned int cnt, flags;
	int fd, ret;

	if (*packs == NULL) {
		*controllen = 0;
		return 0;
	}

	if (*controllen < CMSG_LEN(sizeof(int))) {
		return (int)MSG_CTRUNC;
	}

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	(void)proc_lockSet(&p->lock);

	cmsg = CMSG_FIRSTHDR(control, *controllen);
	cmsg_data = CMSG_DATA(cmsg);

	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	pack = *packs;
	cnt = 0;

	/* unpack and add file descriptors */
	while (pack != NULL && pack->cnt != 0U && *controllen >= CMSG_LEN(sizeof(int) * ((size_t)cnt + 1U))) {
		FDPACK_POP_FILE_AND_FLAGS(pack, file, flags);

		fd = _posix_addOpenFile(p, file, flags);
		if (fd < 0) {
			(void)posix_fileDeref(file);
		}
		else {
			hal_memcpy(cmsg_data, &fd, sizeof(int));
			cmsg_data += sizeof(int);
			++cnt;
		}

		if (pack->cnt == 0U) {
			LIST_REMOVE(packs, pack);

			if (peek != 0U) {
				LIST_ADD(&removed, pack);
				swap(pack->first, pack->cnt);
			}
			else {
				vm_kfree(pack);
			}

			pack = *packs;
		}
	}

	*controllen = cmsg->cmsg_len = CMSG_LEN(sizeof(int) * cnt);
	ret = (*packs == NULL) ? 0 : (int)MSG_CTRUNC;

	if (peek != 0U) {
		pack = removed;
		while (pack != NULL) {
			LIST_REMOVE(&removed, pack);
			LIST_ADD(packs, pack);
			pack = removed;
		}
	}

	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return ret;
}


int fdpass_discard(fdpack_t **packs)
{
	process_info_t *p;
	fdpack_t *pack;
	open_file_t *file;

	p = pinfo_find(process_getPid(proc_current()->process));
	if (p == NULL) {
		return -1;
	}

	(void)proc_lockSet(&p->lock);

	while (*packs != NULL) {
		pack = *packs;
		while (pack->cnt != 0U) {
			FDPACK_POP_FILE(pack, file);
			(void)posix_fileDeref(file);
		}
		LIST_REMOVE(packs, pack);
		vm_kfree(pack);
	}

	(void)proc_lockClear(&p->lock);
	pinfo_put(p);
	return 0;
}
