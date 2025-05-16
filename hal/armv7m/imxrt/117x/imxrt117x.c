/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * i.MX RT1170 basic peripherals control functions
 *
 * Copyright 2017, 2019-2023 Phoenix Systems
 * Author: Aleksander Kaminski, Jan Sikorski, Gerard Swiderski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/hal.h"
#include "hal/armv7m/imxrt/halsyspage.h"

#include "include/errno.h"
#include "include/arch/armv7m/imxrt/11xx/imxrt1170.h"
#include "imxrt117x.h"
#include "config.h"

#include "hal/arm/barriers.h"
#include "hal/arm/scs.h"
#include "hal/arm/rtt.h"

#include <board_config.h>

#define RTWDOG_UNLOCK_KEY  0xd928c520U
#define RTWDOG_REFRESH_KEY 0xb480a602U

#if defined(WATCHDOG)
#if !defined(WATCHDOG_TIMEOUT_MS)
#error "WATCHDOG_TIMEOUT_MS not defined, defaulting to 30000 ms"
/* 1500 ms is the sum of the minimum sensible watchdog timeout (500 ms) and time for WICT interrupt
to fire before watchdog times out (1000 ms) */
#elif (WATCHDOG_TIMEOUT_MS < 1500 || WATCHDOG_TIMEOUT_MS > 128000)
#error "Watchdog timeout out of bounds!"
#endif
#endif


/* clang-format off */

enum { stk_ctrl = 0, stk_load, stk_val, stk_calib };


enum { aipstz_mpr = 0, aipstz_opacr = 16, aipstz_opacr1, aipstz_opacr2, aipstz_opacr3, aipstz_opacr4 };


enum { src_scr = 0, src_srmr, src_sbmr1, src_sbmr2, src_srsr,
	src_gpr1, src_gpr2, src_gpr3, src_gpr4, src_gpr5,
	src_gpr6, src_gpr7, src_gpr8, src_gpr9, src_gpr10,
	src_gpr11, src_gpr12, src_gpr13, src_gpr14, src_gpr15,
	src_gpr16, src_gpr17, src_gpr18, src_gpr19, src_gpr20,
	src_authen = 128, src_ctrl, src_setpoint, src_domain, src_stat };


enum { wdog_wcr = 0, wdog_wsr, wdog_wrsr, wdog_wicr, wdog_wmcr };


enum { rtwdog_cs = 0, rtwdog_cnt, rtwdog_toval, rtwdog_win };

/* clang-format on */


static struct {
	volatile u32 *aips[4];
	volatile u32 *stk;
	volatile u32 *src;
	volatile u16 *wdog1;
	volatile u16 *wdog2;
	volatile u32 *rtwdog3;
	volatile u32 *rtwdog4;
	volatile u32 *iomux_snvs;
	volatile u32 *iomux_lpsr;
	volatile u32 *iomuxc;
	volatile u32 *gpr;
	volatile u32 *lpsrgpr;
	volatile u32 *ccm;

	spinlock_t pltctlSp;

	u32 cpuclk;
} imxrt_common;


void _imxrt_wdgReload(void)
{
	/* If the watchdog was enabled (e.g. by bootrom), then it has to be serviced
	and WATCHDOG flag doesn't matter */
	if ((*(imxrt_common.wdog1 + wdog_wcr) & (1U << 2)) != 0U) {
		*(imxrt_common.wdog1 + wdog_wsr) = 0x5555U;
		hal_cpuDataMemoryBarrier();
		*(imxrt_common.wdog1 + wdog_wsr) = 0xaaaaU;
	}
}


/* IOMUX */


static volatile u32 *_imxrt_IOmuxGetReg(int mux)
{
	if ((mux < pctl_mux_gpio_emc_b1_00) || (mux > pctl_mux_gpio_lpsr_15)) {
		return NULL;
	}

	if (mux < pctl_mux_wakeup) {
		return imxrt_common.iomuxc + 4 + mux - pctl_mux_gpio_emc_b1_00;
	}

	if (mux < pctl_mux_gpio_lpsr_00) {
		return imxrt_common.iomux_snvs + mux - pctl_mux_wakeup;
	}

	return imxrt_common.iomux_lpsr + mux - pctl_mux_gpio_lpsr_00;
}


int _imxrt_setIOmux(int mux, int sion, int mode)
{
	volatile u32 *reg;

	reg = _imxrt_IOmuxGetReg(mux);
	if (reg == NULL) {
		return -EINVAL;
	}

	(*reg) = ((sion == 0 ? 0UL : 1UL) << 4) | ((u32)mode & 0xfU);
	hal_cpuDataMemoryBarrier();

	return EOK;
}


static int _imxrt_getIOmux(int mux, int *sion, int *mode)
{
	u32 t;
	volatile u32 *reg;

	reg = _imxrt_IOmuxGetReg(mux);
	if (reg == NULL) {
		return -EINVAL;
	}

	t = (*reg);
	*sion = ((t & (1U << 4)) == 0U ? 1 : 0);
	*mode = (int)(u32)(t & 0xfU);

	return EOK;
}


static volatile u32 *_imxrt_IOpadGetReg(int pad)
{
	if ((pad < pctl_pad_gpio_emc_b1_00) || (pad > pctl_pad_gpio_lpsr_15)) {
		return NULL;
	}

	if (pad < pctl_pad_test_mode) {
		return imxrt_common.iomuxc + pad + 149 - pctl_pad_gpio_emc_b1_00;
	}

	if (pad < pctl_pad_gpio_lpsr_00) {
		return imxrt_common.iomux_snvs + pad + 13 - pctl_pad_test_mode;
	}

	return imxrt_common.iomux_lpsr + pad + 16 - pctl_pad_gpio_lpsr_00;
}


int _imxrt_setIOpad(int pad, u8 sre, u8 dse, u8 pue, u8 pus, u8 ode, u8 apc)
{
	u32 t;
	volatile u32 *reg;
	u8 pull;

	reg = _imxrt_IOpadGetReg(pad);
	if (reg == NULL) {
		return -EINVAL;
	}

	if ((pad <= pctl_pad_gpio_emc_b2_20) || ((pad >= pctl_pad_gpio_sd_b1_00) && (pad <= pctl_pad_gpio_disp_b1_11))) {
		/* Fields have slightly different meaning... */
		if (pue == 0U) {
			pull = 3U;
		}
		else if (pus != 0U) {
			pull = 1U;
		}
		else {
			pull = 2U;
		}

		t = *reg & ~0x1eU;
		t |= ((dse == 0U ? 0UL : 1UL) << 1) | ((u32)pull << 2) | ((ode == 0U ? 0UL : 1UL) << 4);
	}
	else {
		t = *reg & ~0x1fU;
		t |= ((sre == 0U ? 0UL : 1UL)) | ((dse == 0U ? 0UL : 1UL) << 1) | ((pue == 0U ? 0UL : 1UL) << 2) | ((pus == 0U ? 0UL : 1UL) << 3);

		if (pad <= pctl_pad_gpio_disp_b2_15) {
			t &= ~(1U << 4);
			t |= (ode == 0U ? 0UL : 1UL) << 4;
		}
		else if ((pad >= pctl_pad_wakeup) && (pad <= pctl_pad_gpio_snvs_09)) {
			t &= ~(1U << 6);
			t |= (ode == 0U ? 0UL : 1UL) << 6;
		}
		else if (pad >= pctl_pad_gpio_lpsr_00) {
			t &= ~(1U << 5);
			t |= (ode == 0U ? 0UL : 1UL) << 5;
		}
		else {
			/* MISRA */
			/* pctl_pad_test_mode, pctl_pad_por_b, pctl_pad_onoff - no ode field */
		}
	}

	/*
	 * APC field is not documented. Leave it alone for now.
	 * t &= ~(0xfU << 28);
	 * t |= (apc & 0xfU) << 28;
	 */

	(*reg) = t;
	hal_cpuDataMemoryBarrier();

	return EOK;
}


static int _imxrt_getIOpad(int pad, u8 *sre, u8 *dse, u8 *pue, u8 *pus, u8 *ode, u8 *apc)
{
	u32 t;
	u8 pull;
	volatile u32 *reg;

	reg = _imxrt_IOpadGetReg(pad);
	if (reg == NULL) {
		return -EINVAL;
	}

	t = (*reg);

	if ((pad <= pctl_pad_gpio_emc_b2_20) || ((pad >= pctl_pad_gpio_sd_b1_00) && (pad <= pctl_pad_gpio_disp_b1_11))) {
		pull = (u8)(t >> 2) & 3U;

		if (pull == 3U) {
			*pue = 0;
		}
		else {
			*pue = 1;
			if ((pull & 1U) != 0U) {
				*pus = 1U;
			}
			else {
				*pus = 0U;
			}
		}

		*ode = (u8)(t >> 4) & 1U;
		/* sre field does not apply, leave it alone */
	}
	else {
		*sre = (u8)(t & 1U);
		*pue = (u8)(t >> 2) & 1U;
		*pus = (u8)(t >> 3) & 1U;

		if (pad <= pctl_pad_gpio_disp_b2_15) {
			*ode = (u8)(t >> 4) & 1U;
		}
		else if ((pad >= pctl_pad_wakeup) && (pad <= pctl_pad_gpio_snvs_09)) {
			*ode = (u8)(t >> 6) & 1U;
		}
		else if (pad >= pctl_pad_gpio_lpsr_00) {
			*ode = (u8)(t >> 5) & 1U;
		}
		else {
			/* MISRA */
			/* pctl_pad_test_mode, pctl_pad_por_b, pctl_pad_onoff - no ode field */
		}
	}

	*dse = (u8)(t >> 1) & 1U;
	*apc = (u8)(t >> 28) & 0xfU;

	return EOK;
}


static volatile u32 *_imxrt_IOiselGetReg(int isel, u32 *mask)
{
	if ((isel < pctl_isel_flexcan1_rx) || (isel > pctl_isel_sai4_txsync)) {
		return NULL;
	}

	switch (isel) {
		case pctl_isel_flexcan1_rx:
		case pctl_isel_ccm_enet_qos_ref_clk:
		case pctl_isel_enet_ipg_clk_rmii:
		case pctl_isel_enet_1g_ipg_clk_rmii:
		case pctl_isel_enet_1g_mac0_mdio:
		case pctl_isel_enet_1g_mac0_rxclk:
		case pctl_isel_enet_1g_mac0_rxdata_0:
		case pctl_isel_enet_1g_mac0_rxdata_1:
		case pctl_isel_enet_1g_mac0_rxdata_2:
		case pctl_isel_enet_1g_mac0_rxdata_3:
		case pctl_isel_enet_1g_mac0_rxen:
		case enet_qos_phy_rxer:
		case pctl_isel_flexspi1_dqs_fa:
		case pctl_isel_lpuart1_rxd:
		case pctl_isel_lpuart1_txd:
		case pctl_isel_qtimer1_tmr0:
		case pctl_isel_qtimer1_tmr1:
		case pctl_isel_qtimer2_tmr0:
		case pctl_isel_qtimer2_tmr1:
		case pctl_isel_qtimer3_tmr0:
		case pctl_isel_qtimer3_tmr1:
		case pctl_isel_qtimer4_tmr0:
		case pctl_isel_qtimer4_tmr1:
		case pctl_isel_sdio_slv_clk_sd:
		case pctl_isel_sdio_slv_cmd_di:
		case pctl_isel_sdio_slv_dat0_do:
		case pctl_isel_slv_dat1_irq:
		case pctl_isel_sdio_slv_dat2_rw:
		case pctl_isel_sdio_slv_dat3_cs:
		case pctl_isel_spdif_in1:
		case pctl_isel_can3_canrx:
		case pctl_isel_lpuart12_rxd:
		case pctl_isel_lpuart12_txd:
			(*mask) = 0x3U;
			break;

		default:
			(*mask) = 0x1U;
			break;
	}

	if (isel >= pctl_isel_can3_canrx) {
		return imxrt_common.iomux_lpsr + 32 + isel - pctl_isel_can3_canrx;
	}

	return imxrt_common.iomuxc + 294 + isel - pctl_isel_flexcan1_rx;
}


int _imxrt_setIOisel(int isel, int daisy)
{
	volatile u32 *reg;
	u32 mask;

	reg = _imxrt_IOiselGetReg(isel, &mask);
	if (reg == NULL) {
		return -EINVAL;
	}

	(*reg) = (u32)daisy & mask;
	hal_cpuDataMemoryBarrier();

	return EOK;
}


static int _imxrt_getIOisel(int isel, int *daisy)
{
	volatile u32 *reg;
	u32 mask;

	reg = _imxrt_IOiselGetReg(isel, &mask);
	if (reg == NULL) {
		return -EINVAL;
	}

	*daisy = (int)(u32)((*reg) & mask);

	return EOK;
}


/* SRC */


static void _imxrt_resetSlice(unsigned int index)
{
	*(imxrt_common.src + src_ctrl + 8U * index) |= 1U;
	hal_cpuDataMemoryBarrier();

	while ((*(imxrt_common.src + src_stat + 8U * index) & 1U) != 0U) {
	}
}


/* CCM */

/* MISRA TODO: make args u32? */
int _imxrt_setDevClock(int clock, int div, int mux, int mfd, int mfn, int state)
{
	unsigned int t;
	volatile u32 *reg = imxrt_common.ccm + (clock * 0x20);

	if ((clock < pctl_clk_cm7) || (clock > pctl_clk_ccm_clko2)) {
		return -1;
	}

	t = *reg & ~0x01ff07ffu;
	*reg = t | ((state == 0 ? 1UL : 0UL) << 24) | (((u32)mfn & 0xfU) << 20) | (((u32)mfd & 0xfU) << 16) | (((u32)mux & 0x7U) << 8) | ((u32)div & 0xffU);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	return 0;
}


static int _imxrt_getDevClock(int clock, int *div, int *mux, int *mfd, int *mfn, int *state)
{
	unsigned int t;
	volatile u32 *reg = imxrt_common.ccm + (clock * 0x20);

	if ((clock < pctl_clk_cm7) || (clock > pctl_clk_ccm_clko2)) {
		return -1;
	}

	t = *reg;

	*div = (int)(u32)(t & 0xffU);
	*mux = (int)(u32)((t >> 8) & 0x7U);
	*mfd = (int)(u32)((t >> 16) & 0xfU);
	*mfn = (int)(u32)((t >> 20) & 0xfU);
	*state = (t & (1UL << 24)) == 0U ? 1 : 0;

	return 0;
}


/* MISRA TODO: make args u32? */
int _imxrt_setDirectLPCG(int clock, int state)
{
	u32 t;
	volatile u32 *reg;

	if ((clock < pctl_lpcg_m7) || (clock > pctl_lpcg_uniq_edt_i)) {
		return -EINVAL;
	}

	reg = imxrt_common.ccm + 0x1800 + clock * 0x8;

	t = *reg & ~1u;
	*reg = t | ((u32)state & 1U);

	hal_cpuDataMemoryBarrier();
	hal_cpuInstrBarrier();

	return EOK;
}


int _imxrt_getDirectLPCG(int clock, int *state)
{
	if ((clock < pctl_lpcg_m7) || (clock > pctl_lpcg_uniq_edt_i)) {
		return -EINVAL;
	}

	*state = (int)(u32)(*((volatile u32 *)(imxrt_common.ccm + 0x1800U + (unsigned int)clock * 0x8U)) & 1U);

	return EOK;
}


int _imxrt_setLevelLPCG(int clock, int level)
{
	volatile u32 *reg;

	if ((clock < pctl_lpcg_m7) || (clock > pctl_lpcg_uniq_edt_i)) {
		return -EINVAL;
	}

	if ((level < 0) || (level > 4)) {
		return -EINVAL;
	}

	reg = imxrt_common.ccm + 0x1801 + clock * 0x8;
	*reg = ((u32)level << 28) | ((u32)level << 24) | ((u32)level << 20) | ((u32)level << 16) | (u32)level;

	hal_cpuDataMemoryBarrier();
	hal_cpuInstrBarrier();

	return EOK;
}


/* GPR */


static int _imxrt_setIOgpr(int which, unsigned int what)
{
	/*
	 * GPR19, GPR56 - GPR58, GPR60 - GPR61 don't exist
	 * GPR63, GPR75, GPR76 are read only
	 */
	if ((which < 0) || (which == 19) || ((which > 55) && (which < 62) && (which != 59)) || (which == 63) || (which > 74)) {
		return -EINVAL;
	}

	*(imxrt_common.gpr + which) = what;
	hal_cpuDataSyncBarrier();

	return 0;
}


static int _imxrt_getIOgpr(int which, unsigned int *what)
{
	/*
	 * GPR19, GPR56 - GPR58, GPR60 - GPR61 don't exist
	 */
	if ((which < 0) || (which == 19) || ((which > 55) && (which < 62) && (which != 59)) || (which > 76) || (what == NULL)) {
		return -EINVAL;
	}

	*what = *(imxrt_common.gpr + which);

	return 0;
}


static int _imxrt_setIOlpsrGpr(int which, unsigned int what)
{
	/*
	 * GPR27 - GPR32 don't exist
	 * GPR40 and GPR41 are read only
	 */
	if ((which < 0) || ((which > 26) && (which < 33)) || (which > 39)) {
		return -EINVAL;
	}

	*(imxrt_common.lpsrgpr + which) = what;
	hal_cpuDataSyncBarrier();

	return 0;
}


static int _imxrt_getIOlpsrGpr(int which, unsigned int *what)
{
	/*
	 * GPR27 - GPR32 don't exist
	 */
	if ((which < 0) || ((which > 26) && (which < 33)) || (which > 41) || (what == NULL)) {
		return -EINVAL;
	}

	*what = *(imxrt_common.lpsrgpr + which);

	return 0;
}


static int _imxrt_setSharedGpr(int which, unsigned int what)
{
	if ((which < 0) || (which > 7)) {
		return -EINVAL;
	}

	*(imxrt_common.ccm + 0x1200 + which * 0x8) = what;
	hal_cpuDataSyncBarrier();

	return 0;
}


static int _imxrt_getSharedGpr(int which, unsigned int *what)
{
	if ((which < 0) || (which > 7) || (what == NULL)) {
		return -EINVAL;
	}

	*what = *(imxrt_common.ccm + 0x1200 + which * 0x8);

	return 0;
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	int ret = -EINVAL;
	spinlock_ctx_t sc;
	int div, mux, mfd, mfn, state;
	unsigned int t = 0;
	int sion = 0, mode = 0, daisy = 0;

	hal_spinlockSet(&imxrt_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_devclock:
			if (data->action == pctl_set) {
				ret = _imxrt_setDevClock(data->devclock.dev, data->devclock.div, data->devclock.mux,
						data->devclock.mfd, data->devclock.mfn, data->devclock.state);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getDevClock(data->devclock.dev, &div, &mux, &mfd, &mfn, &state);
				if (ret == 0) {
					data->devclock.div = div;
					data->devclock.mux = mux;
					data->devclock.mfd = mfd;
					data->devclock.mfn = mfn;
					data->devclock.state = state;
				}
			}
			else {
				/* No action required */
			}
			break;

		case pctl_lpcg:
			if (data->action == pctl_set) {
				if (data->lpcg.op == pctl_lpcg_op_direct) {
					ret = _imxrt_setDirectLPCG(data->lpcg.dev, data->lpcg.state);
				}
				else if (data->lpcg.op == pctl_lpcg_op_level) {
					ret = _imxrt_setLevelLPCG(data->lpcg.dev, data->lpcg.state);
				}
				else {
					/* No action required */
				}
			}
			else if (data->action == pctl_get) {
				if (data->lpcg.op == pctl_lpcg_op_direct) {
					ret = _imxrt_getDirectLPCG(data->lpcg.dev, &state);
					if (ret == EOK) {
						data->lpcg.state = state;
					}
				}
			}
			else {
				/* No action required */
			}
			break;

		case pctl_iogpr:
			if (data->action == pctl_set) {
				ret = _imxrt_setIOgpr(data->iogpr.field, data->iogpr.val);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getIOgpr(data->iogpr.field, &t);
				if (ret == 0) {
					data->iogpr.val = t;
				}
			}
			else {
				/* No action required */
			}
			break;

		case pctl_iolpsrgpr:
			if (data->action == pctl_set) {
				ret = _imxrt_setIOlpsrGpr(data->iogpr.field, data->iogpr.val);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getIOlpsrGpr(data->iogpr.field, &t);
				if (ret == 0) {
					data->iogpr.val = t;
				}
			}
			else {
				/* No action required */
			}
			break;

		case pctl_iomux:
			if (data->action == pctl_set) {
				ret = _imxrt_setIOmux(data->iomux.mux, data->iomux.sion, data->iomux.mode);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getIOmux(data->iomux.mux, &sion, &mode);
				data->iomux.sion = sion;
				data->iomux.mode = mode;
			}
			else {
				/* No action required */
			}
			break;

		case pctl_iopad:
			if (data->action == pctl_set) {
				ret = _imxrt_setIOpad(data->iopad.pad, data->iopad.sre, data->iopad.dse, data->iopad.pue,
						data->iopad.pus, data->iopad.ode, data->iopad.apc);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getIOpad(data->iopad.pad, &data->iopad.sre, &data->iopad.dse, &data->iopad.pue,
						&data->iopad.pus, &data->iopad.ode, &data->iopad.apc);
			}
			else {
				/* No action required */
			}
			break;

		case pctl_ioisel:
			if (data->action == pctl_set) {
				ret = _imxrt_setIOisel(data->ioisel.isel, data->ioisel.daisy);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getIOisel(data->ioisel.isel, &daisy);
				data->ioisel.daisy = daisy;
			}
			else {
				/* No action required */
			}
			break;

		case pctl_reboot:
			if (data->action == pctl_set) {
				if (data->reboot.magic == PCTL_REBOOT_MAGIC) {
					_hal_scsSystemReset();
				}
			}
			else if (data->action == pctl_get) {
				data->reboot.reason = syspage->hs.bootReason;
				ret = EOK;
			}
			else {
				/* No action required */
			}
			break;

		case pctl_devcache:
			if (data->action == pctl_set) {
				if (data->devcache.state == 0U) {
					_hal_scsDCacheDisable();
					_hal_scsICacheDisable();
				}
				else {
					_hal_scsDCacheEnable();
					_hal_scsICacheEnable();
				}

				ret = EOK;
			}
			break;

		case pctl_cleanInvalDCache:
			if (data->action == pctl_set) {
				_hal_scsDCacheCleanInvalAddr(data->cleanInvalDCache.addr, data->cleanInvalDCache.sz);
				ret = EOK;
			}
			break;

		case pctl_resetSlice:
			if (data->action == pctl_set) {
				if ((data->resetSlice.index >= (unsigned int)pctl_resetSliceMega) && (data->resetSlice.index <= (unsigned int)pctl_resetSliceCM7Mem)) {
					_imxrt_resetSlice(data->resetSlice.index);
					ret = EOK;
				}
			}
			break;

		case pctl_sharedGpr:
			if (data->action == pctl_set) {
				ret = _imxrt_setSharedGpr(data->iogpr.field, data->iogpr.val);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getSharedGpr(data->iogpr.field, &t);
				if (ret == 0) {
					data->iogpr.val = t;
				}
			}
			else {
				/* No action required */
			}
			break;

		default:
			/* No action required */
			break;
	}

	hal_spinlockClear(&imxrt_common.pltctlSp, &sc);

	return ret;
}


void _imxrt_platformInit(void)
{
	hal_spinlockCreate(&imxrt_common.pltctlSp, "pltctlSp");
}


void _imxrt_init(void)
{
	u32 tmp;

	imxrt_common.aips[0] = (void *)0x40000000U;
	imxrt_common.aips[1] = (void *)0x40400000U;
	imxrt_common.aips[2] = (void *)0x40800000U;
	imxrt_common.aips[3] = (void *)0x40c00000U;
	imxrt_common.ccm = (void *)0x40cc0000U;
	imxrt_common.stk = (void *)0xe000e010U;
	imxrt_common.wdog1 = (void *)0x40030000U;
	imxrt_common.wdog2 = (void *)0x40034000U;
	imxrt_common.rtwdog3 = (void *)0x40038000U;
	imxrt_common.rtwdog4 = (void *)0x40c10000U;
	imxrt_common.src = (void *)0x40c04000U;
	imxrt_common.iomux_snvs = (void *)0x40c94000U;
	imxrt_common.iomux_lpsr = (void *)0x40c08000U;
	imxrt_common.iomuxc = (void *)0x400e8000U;
	imxrt_common.gpr = (void *)0x400e4000U;
	imxrt_common.lpsrgpr = (void *)0x40c0c000U;

	imxrt_common.cpuclk = 696000000U;

	_hal_scsInit();
	_hal_rttInit();

	/* WDOG1 and WDOG2 can't be disabled once enabled */

	/* Enabling the watchdog and setting the timeout are separate actions controlled by WATCHDOG and
	WATCHDOG_TIMEOUT_MS, so it is possible to e.g. change the timeout if the watchdog was already
	enabled by plo or bootrom, but not enabling it if it was disabled. */

#if defined(WATCHDOG_TIMEOUT_MS)
	/* Set the timeout (always possible) */
	tmp = (*(imxrt_common.wdog1 + wdog_wcr) & ~(0xffU << 8));
	*(imxrt_common.wdog1 + wdog_wcr) = tmp | (((WATCHDOG_TIMEOUT_MS - 500U) / 500U) << 8);
	hal_cpuDataMemoryBarrier();
#endif
#if defined(WATCHDOG)
	/* Enable the watchdog */
	*(imxrt_common.wdog1 + wdog_wcr) |= (1U << 2);
	hal_cpuDataMemoryBarrier();
#endif
#if defined(WATCHDOG_TIMEOUT_MS)
	/* Reload the watchdog with a new timeout value in case it was already enabled by
	bootrom/plo and was running with a different timeout */
	_imxrt_wdgReload();
#endif

	/* Disable WDOG3 and WDOG4 in case plo didn't do this */

	if ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1U << 7)) != 0U) {
		/* WDOG3: Unlock rtwdog update */
		*(imxrt_common.rtwdog3 + rtwdog_cnt) = RTWDOG_UNLOCK_KEY;
		hal_cpuDataMemoryBarrier();
		while ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1UL << 11)) == 0U) {
		}

		/* WDOG3: Disable rtwdog, but allow later reconfiguration without reset */
		*(imxrt_common.rtwdog3 + rtwdog_toval) = 0xffffU;
		tmp = (*(imxrt_common.rtwdog3 + rtwdog_cs) & ~(1U << 7));
		*(imxrt_common.rtwdog3 + rtwdog_cs) = tmp | (1U << 5);

		/* WDOG3: Wait until new config takes effect */
		while ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1UL << 10)) == 0U) {
		}

		/* WDOG3: Wait until registers are locked (in case low power mode will be used promptly) */
		while ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1UL << 11)) != 0U) {
		}
	}

	if ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1U << 7)) != 0U) {
		/* WDOG4: Unlock rtwdog update */
		*(imxrt_common.rtwdog4 + rtwdog_cnt) = RTWDOG_UNLOCK_KEY;
		hal_cpuDataMemoryBarrier();
		while ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1UL << 11)) == 0U) {
		}

		/* WDOG4: Disable rtwdog, but allow later reconfiguration without reset */
		*(imxrt_common.rtwdog4 + rtwdog_toval) = 0xffffU;
		tmp = (*(imxrt_common.rtwdog4 + rtwdog_cs) & ~(1U << 7));
		*(imxrt_common.rtwdog4 + rtwdog_cs) = tmp | (1U << 5);

		/* WDOG4: Wait until new config takes effect */
		while ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1UL << 10)) == 0U) {
		}

		/* WDOG4: Wait until registers are locked (in case low power mode will be used promptly) */
		while ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1UL << 11)) != 0U) {
		}
	}

	/* Enable system HP timer clock gate, select SYS_PLL3_DIV2 as BUS clk */
	(void)_imxrt_setDevClock(GPT_BUS_CLK, 0, 4, 0, 0, 1);

	/* Enable FPU */
	_hal_scsFPUSet(1);
}
