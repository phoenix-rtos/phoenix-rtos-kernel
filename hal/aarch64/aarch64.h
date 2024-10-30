/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * AArch64 related routines
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_AARCH64_H_
#define _HAL_AARCH64_H_

#include "hal/types.h"


#define sysreg_write(sysreg, val) \
	({ \
		unsigned long __v = (unsigned long)(val); \
		__asm__ volatile( \
			"msr " #sysreg ", %0" \
			: \
			: "r"(__v) \
			: "memory"); \
	})


#define sysreg_read(sysreg) \
	({ \
		register unsigned long __v; \
		__asm__ volatile( \
			"mrs %0, " #sysreg \
			: "=r"(__v) \
			: \
			: "memory"); \
		__v; \
	})


/* Barriers */

static inline void hal_cpuDataMemoryBarrier(void)
{
	asm volatile ("dmb ish");
}


static inline void hal_cpuDataSyncBarrier(void)
{
	asm volatile ("dsb ish");
}


static inline void hal_cpuDataSyncBarrierSys(void)
{
	asm volatile ("dsb sy");
}


static inline void hal_cpuInstrBarrier(void)
{
	asm volatile ("isb");
}


/* Memory Management */


/* Invalidate all instruction caches to PoU. Also flushes branch target cache */
extern void hal_cpuICacheInval(void);


/* Clean Data or Unified cache line by MVA to PoC */
extern void hal_cpuCleanDataCache(ptr_t vstart, ptr_t vend);


/* Invalidate Data or Unified cache line by MVA to PoC */
extern void hal_cpuInvalDataCache(ptr_t vstart, ptr_t vend);


/* Clean and Invalidate Data or Unified cache line by MVA to PoC */
extern void hal_cpuFlushDataCache(ptr_t vstart, ptr_t vend);


/* Invalidate TLB entries by ASID Match */
static inline void hal_tlbInvalASID(asid_t asid)
{
	u64 arg = (u64)asid << 48;
	asm volatile("tlbi aside1, %0" : : "r"(arg));
}


/* Invalidate Unified TLB by VA (all ASIDs) */
static inline void hal_tlbInvalVA(ptr_t vaddr)
{
	u64 arg = (vaddr >> 12) & ((1uL << 44) - 1);
	asm volatile("tlbi vaae1, %0" : : "r"(arg));
}


/* Invalidate Unified TLB by VA (selected ASID) */
static inline void hal_tlbInvalVAASID(ptr_t vaddr, asid_t asid)
{
	u64 arg = ((vaddr >> 12) & ((1uL << 44) - 1)) | ((u64)asid << 48);
	asm volatile("tlbi vae1, %0" : : "r"(arg));
}


/* Invalidate entire Unified TLB */
static inline void hal_tlbInvalAll(void)
{
	asm volatile("tlbi vmalle1");
}


/* Invalidate TLB entries by ASID Match (broadcast to Inner Shareable domain) */
static inline void hal_tlbInvalASID_IS(asid_t asid)
{
	u64 arg = (u64)asid << 48;
	asm volatile("tlbi aside1is, %0" : : "r"(arg));
}


/* Invalidate Unified TLB by VA (all ASIDs) (broadcast to Inner Shareable domain) */
static inline void hal_tlbInvalVA_IS(ptr_t vaddr)
{
	u64 arg = (vaddr >> 12) & ((1uL << 44) - 1);
	asm volatile("tlbi vaae1is, %0" : : "r"(arg));
}


/* Invalidate Unified TLB by VA (selected ASID) (broadcast to Inner Shareable domain) */
static inline void hal_tlbInvalVAASID_IS(ptr_t vaddr, asid_t asid)
{
	u64 arg = ((vaddr >> 12) & ((1uL << 44) - 1)) | ((u64)asid << 48);
	asm volatile("tlbi vae1is, %0" : : "r"(arg));
}


/* Invalidate entire Unified TLB (broadcast to Inner Shareable domain) */
static inline void hal_tlbInvalAll_IS(void)
{
	asm volatile("tlbi vmalle1is");
}


/* Read Translation Table Base Register 0 and get only translation table physical address */
static inline addr_t hal_cpuGetTranslationBase(void)
{
	return sysreg_read(ttbr0_el1) & ((1uL << 48) - (1uL << 1));
}


/* Set Translation Table Base Register 0 to translation table address and ASID */
static inline void hal_cpuSetTranslationBase(addr_t addr, asid_t asid)
{
	sysreg_write(ttbr0_el1, addr | ((u64)asid << 48));
}


static inline asid_t hal_getCurrentAsid(void)
{
	return (sysreg_read(ttbr0_el1) >> 48) & 0xffff;
}

/* Core Management */

struct aarch64_proc_id {
	u64 mmfr0; /* ID_AA64MMFR0_EL1 */
	u64 pfr0; /* ID_AA64PFR0_EL1 */
	u64 isar0; /* ID_AA64ISAR0_EL1 */
	u32 dfr0; /* ID_AA64DFR0_EL1 */
	u32 midr; /* MIDR_EL1 */
};

extern void hal_cpuGetProcID(struct aarch64_proc_id *out);

#endif
