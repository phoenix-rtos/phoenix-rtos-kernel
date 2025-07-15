/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for TDA4VM
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_TDA4VM_H_
#define _HAL_TDA4VM_H_

#include "hal/cpu.h"
#include "include/arch/armv7r/tda4vm/tda4vm.h"


typedef struct {
	u32 mult_int;
	u32 mult_frac;
	u8 pre_div;
	u8 post_div1;
	u8 post_div2;
	char is_enabled;
} tda4vm_clk_pll_t;


typedef struct {
	u32 flags;       /* Bitfield of TDA4VM_GPIO_* flags */
	u8 debounce_idx; /* Debounce period selection */
	u8 mux;          /* Pad mux selection */
} tda4vm_pinConfig_t;


/* Get configuration of selected PLL. Returns 0 on success, < 0 on failure. */
extern int tda4vm_getPLL(unsigned pll, tda4vm_clk_pll_t *config);


/* Get frequency in Hz of selected PLL after being divided by the selected HSDIV.
 * Returns 0 if selected PLL and HSDIV combination doesn't exist or value cannot be computed.
 * Depends on `WKUP_HFOSC0_HZ` and `HFOSC1_HZ` to be set correctly. */
extern u64 tda4vm_getFrequency(unsigned pll, unsigned hsdiv);

/* Do warm reset. Software POR is not possible on this platform. */
extern void tda4vm_warmReset(void);


extern int tda4vm_setPinConfig(unsigned pin, const tda4vm_pinConfig_t *config);


extern int tda4vm_getPinConfig(unsigned pin, tda4vm_pinConfig_t *config);


extern int tda4vm_setDebounceConfig(unsigned idx, unsigned period);


/* Use Region-based Address Translation (RAT) to map system memory.
 * `entry` is the entry within RAT module that will store this translation.
 * `cpuAddr` start of the address range within CPU's address range.
 * `physAddr` is address range within system memory's address range.
 * `logSize` is log2 of region size such that region size == 1 << `logSize`.
 * Note: some memory (such as ATCM/BTCM) cannot be mapped in this manner. */
extern int tda4vm_RATMapMemory(unsigned entry, addr_t cpuAddr, u64 physAddr, u32 logSize);


extern void tda4vm_RATUnmapMemory(unsigned entry);


extern int tda4vm_setClksel(unsigned sel, unsigned val);


/* Returns < 0 on error, otherwise the clksel setting */
extern int tda4vm_getClksel(unsigned sel);


extern int tda4vm_setClkdiv(unsigned sel, unsigned val);


/* Returns < 0 on error, otherwise the clksel setting */
extern int tda4vm_getClkdiv(unsigned sel);


#endif
