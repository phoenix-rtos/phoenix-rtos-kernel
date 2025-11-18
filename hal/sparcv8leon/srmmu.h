/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * SPARC reference MMU routines
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PH_HAL_SRMMU_H_
#define _PH_HAL_SRMMU_H_


/* TLB flush types
 * More info about flushing behaviour: SPARC Architecture Manual V8, pages 245-246
 */

#define TLB_FLUSH_L3  0U /* Level 3 PTE */
#define TLB_FLUSH_L2  1U /* Level 2 & 3 PTE/PTDs */
#define TLB_FLUSH_L1  2U /* Level 1, 2 & 3 PTE/PTDs */
#define TLB_FLUSH_CTX 3U /* Level 0, 1, 2 & 3 PTE/PTDs */
#define TLB_FLUSH_ALL 4U /* All PTEs/PTDs */

/* Address Space Identifiers */

#define ASI_FORCE_CACHE_MISS 0x01U
#define ASI_CACHE_CTRL       0x02U
#define ASI_ICACHE_TAGS      0x0cU
#define ASI_ICACHE_DATA      0x0dU
#define ASI_DCACHE_TAGS      0x0eU
#define ASI_DCACHE_DATA      0x0fU
#define ASI_FLUSH_IDCACHE    0x10U /* Writing will flush I and D cache */
#define ASI_FLUSH_DCACHE     0x11U /* Writing will flush D cache */
#define ASI_FLUSH_ALL        0x18U /* Writing will flush TLB, I and D cache */
#define ASI_MMU_REGS         0x19U
#define ASI_MMU_BYPASS       0x1cU

/* MMU register addresses */

#define MMU_CTRL       0x0U
#define MMU_CTX_PTR    0x100U
#define MMU_CTX        0x200U
#define MMU_FAULT_STS  0x300U
#define MMU_FAULT_ADDR 0x400U


#ifndef __ASSEMBLY__


#include "hal/types.h"


#ifndef NOMMU


void hal_srmmuFlushTLB(const void *vaddr, u8 type);


u32 hal_srmmuGetFaultSts(void);


u32 hal_srmmuGetFaultAddr(void);


void hal_srmmuSetContext(u32 ctx);


u32 hal_srmmuGetContext(void);


#endif


#endif


#endif
