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


#ifndef _HAL_SRMMU_H_
#define _HAL_SRMMU_H_


/* TLB flush types
 * More info about flushing behaviour: SPARC Architecture Manual V8, pages 245-246
 */

#define TLB_FLUSH_L3  0 /* Level 3 PTE */
#define TLB_FLUSH_L2  1 /* Level 2 & 3 PTE/PTDs */
#define TLB_FLUSH_L1  2 /* Level 1, 2 & 3 PTE/PTDs */
#define TLB_FLUSH_CTX 3 /* Level 0, 1, 2 & 3 PTE/PTDs */
#define TLB_FLUSH_ALL 4 /* All PTEs/PTDs */

/* Address Space Identifiers */

#define ASI_FORCE_CACHE_MISS 0x01
#define ASI_CACHE_CTRL       0x02
#define ASI_ICACHE_TAGS      0x0c
#define ASI_ICACHE_DATA      0x0d
#define ASI_DCACHE_TAGS      0x0e
#define ASI_DCACHE_DATA      0x0f
#define ASI_FLUSH_IDCACHE    0x10 /* Writing will flush I and D cache */
#define ASI_FLUSH_DCACHE     0x11 /* Writing will flush D cache */
#define ASI_FLUSH_ALL        0x18 /* Writing will flush TLB, I and D cache */
#define ASI_MMU_REGS         0x19
#define ASI_MMU_BYPASS       0x1c

/* MMU register addresses */

#define MMU_CTRL       0x0u
#define MMU_CTX_PTR    0x100u
#define MMU_CTX        0x200u
#define MMU_FAULT_STS  0x300u
#define MMU_FAULT_ADDR 0x400u


#ifndef __ASSEMBLY__


#include <arch/types.h>


void hal_srmmuFlushTLB(const void *vaddr, u8 type);


u32 hal_srmmuGetFaultSts(void);


u32 hal_srmmuGetFaultAddr(void);


void hal_srmmuSetContext(u32 ctx);


u32 hal_srmmuGetContext(void);


#endif


#endif
