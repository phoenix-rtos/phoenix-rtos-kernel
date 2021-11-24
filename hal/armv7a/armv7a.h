
#ifndef _HAL_ARMV7A_H_
#define _HAL_ARMV7A_H_

#include <arch/types.h>


/* memory management */

extern void hal_cpuFlushDataCache(void *vaddr);


extern void hal_cpuInvalDataCache(void *vaddr);


extern void hal_cpuCleanDataCache(void *vaddr);


extern void hal_cpuInvalASID(u8 asid);


extern void hal_cpuInvalVA(u32 vaddr);


extern void hal_cpuInvalTLB(void);


extern void hal_cpuBranchInval(void);


extern void hal_cpuICacheInval(void);


extern addr_t hal_cpuGetUserTT(void);


extern void hal_cpuSetUserTT(addr_t tt);


extern void hal_cpuSetContextId(u32 id);


extern u32 hal_cpuGetContextId(void);


/* core management */

extern u32 hal_cpuGetMIDR(void);


extern u32 hal_cpuGetPFR0(void);


extern u32 hal_cpuGetPFR1(void);

#endif
