#include "hab_api.h"
#include "include/errno.h"
#include "lib/lib.h"
#include "vm/vm.h"
#include "hab_rvt.h"
#include "proc/proc.h"
#include "include/arch/armv7a/imx6ull/imx6ull.h"

#define MASK_PAGE (SIZE_PAGE - 1U)

typedef struct {
	hab_hdr_t hdr;
	u32 entry;
	u32 reserved1;
	u32 dcd;
	u32 boot_data;
	u32 self;
	u32 csf;
	u32 reserved2;
} hab_ivt_t;

#define ntoh16(x) ((((x) << 8U) & 0xff00U) | (((x) >> 8U) & 0xffU))
#define ntoh32(x) ((ntoh16(x) << 16U) | ntoh16((x) >> 16U))


static struct {
	spinlock_t lock;
	u32 minLoadAddr;
	u32 maxLoadAddr;
	void *currentMapMin;
	size_t currentMapSize;
	u8 initialized;
	vm_map_t map;
	hab_rvt_t rvt;
} hab_api_common;


/* Address ranges required by the HAB code. */
static const struct {
	ptr_t start;
	ptr_t end;
	vm_prot_t prot;
	vm_flags_t flags;
} hab_api_memMaps[] = {
	{ SIZE_PAGE, 0x00018000U, PROT_READ | PROT_EXEC, 0 },             /* BootROM (except lowest page) */
	{ 0x00900000U, 0x00920000U, PROT_READ | PROT_WRITE, 0 },          /* OCRAM */
	{ 0x02000000U, 0x02300000U, PROT_READ | PROT_WRITE, MAP_DEVICE }, /* AIPS-1, AIPS-2, AIPS-3 */
};


static void _hab_api_mapSwitchIn(void)
{
	pmap_switch(&hab_api_common.map.pmap);
}


static void _hab_api_mapSwitchOut(void)
{
	thread_t *proc = proc_current();
	if ((proc != NULL) && (proc->process != NULL) && (proc->process->pmapp != NULL)) {
		pmap_switch(proc->process->pmapp);
	}
}


int _hab_api_preinit(u32 minLoadAddr, u32 maxLoadAddr)
{
	hal_memset(&hab_api_common, 0, sizeof(hab_api_common));
	hal_spinlockCreate(&hab_api_common.lock, "hab_api");
	if ((minLoadAddr < VADDR_MIN) ||
			(maxLoadAddr <= VADDR_MIN) ||
			(minLoadAddr >= VADDR_USR_MAX) ||
			(maxLoadAddr > VADDR_USR_MAX) ||
			(minLoadAddr >= maxLoadAddr)) {
		return -EINVAL;
	}

	hab_api_common.minLoadAddr = minLoadAddr;
	hab_api_common.maxLoadAddr = maxLoadAddr;
	return 0;
}


static int _hab_api_init(void)
{
	int ret;
	size_t i;
	void *vaddr;
	hab_hdr_t hdr;
	size_t habSize;

	if (hab_api_common.initialized != 0U) {
		return 0;
	}

	if ((hab_api_common.minLoadAddr == 0U) || (hab_api_common.maxLoadAddr == 0U)) {
		return -ENOSYS;
	}

	ret = vm_mapCreate(&hab_api_common.map, (void *)VADDR_MIN, (void *)VADDR_USR_MAX);
	if (ret < 0) {
		return ret;
	}

	/* Map the required address ranges 1:1 with physical memory */
	for (i = 0; i < sizeof(hab_api_memMaps) / sizeof(hab_api_memMaps[0]); i++) {
		vaddr = vm_mmap(
				&hab_api_common.map,
				(void *)hab_api_memMaps[i].start,
				NULL,
				hab_api_memMaps[i].end - hab_api_memMaps[i].start,
				hab_api_memMaps[i].prot,
				VM_OBJ_PHYSMEM,
				(off_t)hab_api_memMaps[i].start,
				hab_api_memMaps[i].flags);
		if (vaddr != (void *)hab_api_memMaps[i].start) {
			return -ENOMEM;
		}
	}

	/*
	 * We need to map the bottom page separately by directly calling pmap_enter().
	 * This is because trying to map to virtual address 0 causes vm_mmap() to fail.
	 * We want to use vm_mmap() first, because it will do a lot of heavy lifting like allocating page tables.
	 */
	ret = pmap_enter(&hab_api_common.map.pmap, 0U, (void *)0U, PGHD_READ | PGHD_EXEC | PGHD_PRESENT, NULL);
	if (ret < 0) {
		return ret;
	}

	_hab_api_mapSwitchIn();
	hal_memcpy(&hab_api_common.rvt, HAB_ADDR, sizeof(hab_api_common.rvt));
	hdr = ntoh32(hab_api_common.rvt.hdr);
	if (((hdr >> 24) & 0xffu) != HAB_TAG_RVT) {
		_hab_api_mapSwitchOut();
		return -ENOSYS;
	}

	if (((hdr >> 4) & 0xfu) != HAB_MAJOR_VERSION) {
		_hab_api_mapSwitchOut();
		return -ENOSYS;
	}

	habSize = (hdr >> 8) & 0xffffu;
	/* Ensure any function pointers past-the-end of the ROM vector table are set to NULL */
	if (habSize < sizeof(hab_api_common.rvt)) {
		hal_memset(((u8 *)&hab_api_common.rvt) + habSize, 0, sizeof(hab_api_common.rvt) - habSize);
	}

	if ((hab_api_common.rvt.authenticate_image_no_dcd == NULL) && (hab_api_common.rvt.authenticate_image == NULL)) {
		_hab_api_mapSwitchOut();
		return -ENOSYS;
	}

	hab_api_common.initialized = 1U;
	_hab_api_mapSwitchOut();
	return 0;
}


static int _hab_api_ctxEnter(spinlock_ctx_t *sc)
{
	int ret = 0;
	hal_spinlockSet(&hab_api_common.lock, sc);

	if (hab_api_common.initialized == 0U) {
		ret = _hab_api_init();
	}

	return ret;
}


static void _hab_api_ctxExit(spinlock_ctx_t *sc)
{
	if (hab_api_common.currentMapSize != 0U) {
		(void)vm_munmap(&hab_api_common.map, hab_api_common.currentMapMin, hab_api_common.currentMapSize);
		hab_api_common.currentMapMin = NULL;
		hab_api_common.currentMapSize = 0U;
	}

	hal_spinlockClear(&hab_api_common.lock, sc);
}


static int _hab_api_remap(const void *src, void *dst, size_t size, int rw)
{
	const vm_prot_t prot = (rw != 0) ? (PROT_READ | PROT_WRITE) : (PROT_READ);
	const size_t maxSize = hab_api_common.maxLoadAddr - hab_api_common.minLoadAddr;
	const size_t offset = (ptr_t)dst & MASK_PAGE;
	ptr_t srcStart = (ptr_t)src & ~MASK_PAGE;
	ptr_t dstStart = (ptr_t)dst & ~MASK_PAGE;
	int ret;
	void *retMmap;
	size_t i;
	thread_t *proc;
	pmap_t *pmapp;
	addr_t pa;

	size += (offset == 0U) ? 0U : (SIZE_PAGE - offset);
	size = round_page(size);
	if ((size > maxSize) ||
			(dstStart < hab_api_common.minLoadAddr) ||
			((dstStart + size) >= hab_api_common.maxLoadAddr)) {
		return -EINVAL;
	}

	proc = proc_current();
	if ((proc != NULL) && (proc->process != NULL) && (proc->process->pmapp != NULL)) {
		pmapp = proc->process->pmapp;
		/* TODO: should this check be done by caller? */
		ret = vm_mapBelongs(proc->process, src, size);
		if (ret < 0) {
			return -EFAULT;
		}
	}
	else if (src >= (void *)VADDR_KERNEL) {
		/*
		 * Data is in kernel space; this means it's available from any pmap.
		 * We can't give a NULL pointer to pmap_resolve(), so just give our own pmap.
		 */
		pmapp = &hab_api_common.map.pmap;
	}
	else {
		/* No process running, cannot determine which pmap contains data. */
		return -ENOMEM;
	}

	ret = 0;
	for (i = 0; i < size; i += SIZE_PAGE) {
		/* TODO: this could be optimized for contiguous blocks of physical memory */
		pa = pmap_resolve(pmapp, (void *)(srcStart + i));
		if (pa == 0) {
			ret = -EFAULT;
			break;
		}

		pa &= ~MASK_PAGE;
		retMmap = vm_mmap(&hab_api_common.map, (void *)(dstStart + i), NULL, SIZE_PAGE, prot, VM_OBJ_PHYSMEM, (off_t)pa, 0);
		if (retMmap == NULL) {
			ret = -ENOMEM;
			break;
		}
	}

	hab_api_common.currentMapMin = (void *)dstStart;
	hab_api_common.currentMapSize = i;
	return ret;
}


static void *_hab_api_checkLoadAddr(const void *data, size_t size, size_t ivtOffset)
{
	const size_t maxSize = hab_api_common.maxLoadAddr - hab_api_common.minLoadAddr;
	ptr_t loadAddr, loadAddrEnd;
	hab_ivt_t ivt;
	if ((data == NULL) || (size > maxSize) || (size < sizeof(hab_ivt_t)) || (ivtOffset > (size - sizeof(hab_ivt_t)))) {
		return NULL;
	}

	hal_memcpy(&ivt, (const u8 *)data + ivtOffset, sizeof(ivt));
	ivt.hdr = ntoh32(ivt.hdr);
	if (((ivt.hdr >> 24) & 0xffU) != HAB_TAG_IVT) {
		return NULL;
	}

	/* IVT size (must be exactly 32 bytes) */
	if (((ivt.hdr >> 8) & 0xffffU) != 0x20U) {
		return NULL;
	}

	if (((ivt.hdr >> 4) & 0xfU) != HAB_MAJOR_VERSION) {
		return NULL;
	}

	loadAddr = ivt.self - ivtOffset;
	loadAddrEnd = loadAddr + size;
	if ((loadAddr < hab_api_common.minLoadAddr) ||
			(loadAddr >= hab_api_common.maxLoadAddr) ||
			(loadAddrEnd >= hab_api_common.maxLoadAddr) ||
			(loadAddr > loadAddrEnd)) {
		return NULL;
	}

	if ((loadAddr & MASK_PAGE) != ((ptr_t)data & MASK_PAGE)) {
		/* We can remap only with the granularity of SIZE_PAGE */
		return NULL;
	}

	if ((hab_api_common.rvt.authenticate_image_no_dcd == NULL) && (ivt.dcd != 0U)) {
		/*
		 * DCD can only be executed at system boot - ensure that there's either "skip executing DCD" function
		 * or there is no DCD data to execute.
		 */
		return NULL;
	}

	return (void *)loadAddr;
}


static int _hab_api_reportAll(hab_status_t *status, hab_config_t *cfg, hab_state_t *state, u8 *eventsBuf, size_t *bufSize)
{
	hab_status_t ret;
	size_t i, remainingSize, size;

	ret = hab_api_common.rvt.report_status(cfg, state);
	*status = ret;
	if (eventsBuf != NULL && bufSize != NULL) {
		if (ret != HAB_SUCCESS) {
			remainingSize = *bufSize;
			i = 0U;
			while (remainingSize > 0U) {
				size = remainingSize;
				ret = hab_api_common.rvt.report_event(HAB_STS_ANY, i, eventsBuf, &size);
				if ((ret != HAB_SUCCESS) || (size > remainingSize)) {
					break;
				}

				eventsBuf += size;
				remainingSize -= size;
				i++;
			}

			*bufSize = *bufSize - remainingSize;
		}
		else {
			*bufSize = 0U;
		}
	}

	return 0;
}


int hab_api_reportAll(hab_status_t *status, hab_config_t *cfg, hab_state_t *state, u8 *eventsBuf, size_t *bufSize)
{
	spinlock_ctx_t sc;
	int ret;
	void *bufMapped;

	ret = _hab_api_ctxEnter(&sc);
	if (ret < 0) {
		_hab_api_ctxExit(&sc);
		return ret;
	}

	if ((void *)eventsBuf >= (void *)VADDR_KERNEL) {
		/* Buffer is in kernel space, no remapping is necessary */
		bufMapped = eventsBuf;
	}
	else if (eventsBuf != NULL) {
		bufMapped = (void *)(hab_api_common.minLoadAddr + ((ptr_t)eventsBuf & MASK_PAGE));
		ret = _hab_api_remap(eventsBuf, bufMapped, *bufSize, 1);
		if (ret < 0) {
			_hab_api_ctxExit(&sc);
			return ret;
		}
	}
	else {
		bufMapped = NULL;
	}

	_hab_api_mapSwitchIn();
	ret = _hab_api_reportAll(status, cfg, state, bufMapped, bufSize);
	_hab_api_mapSwitchOut();
	_hab_api_ctxExit(&sc);
	return ret;
}


static int _hab_api_verify(void *loadAddr, size_t size, size_t ivtOffset)
{
	hab_status_t sts;
	hab_image_entry_f verifyPtr = NULL;

	sts = hab_api_common.rvt.entry();
	if (sts == HAB_FAILURE) {
		return -EPERM;
	}
	else {
		/* No action necessary, verification can continue */
	}

	if (hab_api_common.rvt.authenticate_image_no_dcd != NULL) {
		verifyPtr = hab_api_common.rvt.authenticate_image_no_dcd(0, ivtOffset, &loadAddr, &size, NULL);
	}
	else if (hab_api_common.rvt.authenticate_image != NULL) {
		verifyPtr = hab_api_common.rvt.authenticate_image(0, ivtOffset, &loadAddr, &size, NULL);
	}
	else {
		/* This should never happen, we checked for it during init */
		return -ENOSYS;
	}

	sts = hab_api_common.rvt.exit();
	return ((sts == HAB_FAILURE) || (verifyPtr == NULL)) ? -EPERM : 0;
}


int hab_api_verify(const void *data, size_t size, size_t ivtOffset)
{
	spinlock_ctx_t sc;
	int ret = 0;
	void *loadAddr;

	ret = _hab_api_ctxEnter(&sc);
	if (ret < 0) {
		_hab_api_ctxExit(&sc);
		return ret;
	}

	loadAddr = _hab_api_checkLoadAddr(data, size, ivtOffset);
	if (loadAddr == NULL) {
		_hab_api_ctxExit(&sc);
		return -EINVAL;
	}

	ret = _hab_api_remap(data, loadAddr, size, 0);
	if (ret < 0) {
		_hab_api_ctxExit(&sc);
		return ret;
	}

	_hab_api_mapSwitchIn();
	ret = _hab_api_verify(loadAddr, size, ivtOffset);
	_hab_api_mapSwitchOut();
	_hab_api_ctxExit(&sc);
	return ret;
}
