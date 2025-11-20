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


#include "hal/console.h"
#include "lib/lib.h"

#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/armv7m/imxrt/halsyspage.h"

#include "include/errno.h"
#include "include/arch/armv7m/imxrt/11xx/imxrt1170.h"
#include "imxrt117x.h"
#include "config.h"

#include "hal/arm/barriers.h"
#include "hal/arm/scs.h"

#include <board_config.h>

#define RTWDOG_UNLOCK_KEY  0xd928c520u
#define RTWDOG_REFRESH_KEY 0xb480a602u

#if defined(WATCHDOG) && !defined(WATCHDOG_TIMEOUT_MS)
#define WATCHDOG_TIMEOUT_MS (30000)
#warning "WATCHDOG_TIMEOUT_MS not defined, defaulting to 30000 ms"
#endif

/* 1500 ms is the sum of the minimum sensible watchdog timeout (500 ms) and time for WICT interrupt
to fire before watchdog times out (1000 ms) */
#if defined(WATCHDOG) && (WATCHDOG_TIMEOUT_MS < 1500 || WATCHDOG_TIMEOUT_MS > 128000)
#error "Watchdog timeout out of bounds!"
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


struct {
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
	if ((*(imxrt_common.wdog1 + wdog_wcr) & (1u << 2)) != 0u) {
		*(imxrt_common.wdog1 + wdog_wsr) = 0x5555;
		hal_cpuDataMemoryBarrier();
		*(imxrt_common.wdog1 + wdog_wsr) = 0xaaaa;
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


int _imxrt_setIOmux(int mux, char sion, char mode)
{
	volatile u32 *reg;

	reg = _imxrt_IOmuxGetReg(mux);
	if (reg == NULL) {
		return -EINVAL;
	}

	(*reg) = (!!sion << 4) | (mode & 0xf);
	hal_cpuDataMemoryBarrier();

	return EOK;
}


static int _imxrt_getIOmux(int mux, char *sion, char *mode)
{
	u32 t;
	volatile u32 *reg;

	reg = _imxrt_IOmuxGetReg(mux);
	if (reg == NULL) {
		return -EINVAL;
	}

	t = (*reg);
	*sion = !!(t & (1 << 4));
	*mode = t & 0xf;

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


int _imxrt_setIOpad(int pad, char sre, char dse, char pue, char pus, char ode, char apc)
{
	u32 t;
	volatile u32 *reg;
	char pull;

	reg = _imxrt_IOpadGetReg(pad);
	if (reg == NULL) {
		return -EINVAL;
	}

	if ((pad <= pctl_pad_gpio_emc_b2_20) || ((pad >= pctl_pad_gpio_sd_b1_00) && (pad <= pctl_pad_gpio_disp_b1_11))) {
		/* Fields have slightly diffrent meaning... */
		if (pue == 0) {
			pull = 3;
		}
		else if (pus != 0) {
			pull = 1;
		}
		else {
			pull = 2;
		}

		t = *reg & ~0x1e;
		t |= (!!dse << 1) | (pull << 2) | (!!ode << 4);
	}
	else {
		t = *reg & ~0x1f;
		t |= (!!sre) | (!!dse << 1) | (!!pue << 2) | (!!pus << 3);

		if (pad <= pctl_pad_gpio_disp_b2_15) {
			t &= ~(1 << 4);
			t |= !!ode << 4;
		}
		else if ((pad >= pctl_pad_wakeup) && (pad <= pctl_pad_gpio_snvs_09)) {
			t &= ~(1 << 6);
			t |= !!ode << 6;
		}
		else if (pad >= pctl_pad_gpio_lpsr_00) {
			t &= ~(1 << 5);
			t |= !!ode << 5;
		}
		else {
			/* MISRA */
			/* pctl_pad_test_mode, pctl_pad_por_b, pctl_pad_onoff - no ode field */
		}
	}

	/* APC field is not documented. Leave it alone for now. */
	//t &= ~(0xf << 28);
	//t |= (apc & 0xf) << 28;

	(*reg) = t;
	hal_cpuDataMemoryBarrier();

	return EOK;
}


static int _imxrt_getIOpad(int pad, char *sre, char *dse, char *pue, char *pus, char *ode, char *apc)
{
	u32 t;
	char pull;
	volatile u32 *reg;

	reg = _imxrt_IOpadGetReg(pad);
	if (reg == NULL) {
		return -EINVAL;
	}

	t = (*reg);

	if ((pad <= pctl_pad_gpio_emc_b2_20) || ((pad >= pctl_pad_gpio_sd_b1_00) && (pad <= pctl_pad_gpio_disp_b1_11))) {
		pull = (t >> 2) & 3;

		if (pull == 3) {
			*pue = 0;
		}
		else {
			*pue = 1;
			if ((pull & 1) != 0) {
				*pus = 1;
			}
			else {
				*pus = 0;
			}
		}

		*ode = (t >> 4) & 1;
		/* sre field does not apply, leave it alone */
	}
	else {
		*sre = t & 1;
		*pue = (t >> 2) & 1;
		*pus = (t >> 3) & 1;

		if (pad <= pctl_pad_gpio_disp_b2_15) {
			*ode = (t >> 4) & 1;
		}
		else if ((pad >= pctl_pad_wakeup) && (pad <= pctl_pad_gpio_snvs_09)) {
			*ode = (t >> 6) & 1;
		}
		else if (pad >= pctl_pad_gpio_lpsr_00) {
			*ode = (t >> 5) & 1;
		}
		else {
			/* MISRA */
			/* pctl_pad_test_mode, pctl_pad_por_b, pctl_pad_onoff - no ode field */
		}
	}

	*dse = (t >> 1) & 1;
	*apc = (t >> 28) & 0xf;

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
			(*mask) = 0x3;
			break;

		default:
			(*mask) = 0x1;
			break;
	}

	if (isel >= pctl_isel_can3_canrx) {
		return imxrt_common.iomux_lpsr + 32 + isel - pctl_isel_can3_canrx;
	}

	return imxrt_common.iomuxc + 294 + isel - pctl_isel_flexcan1_rx;
}


int _imxrt_setIOisel(int isel, char daisy)
{
	volatile u32 *reg;
	u32 mask;

	reg = _imxrt_IOiselGetReg(isel, &mask);
	if (reg == NULL) {
		return -EINVAL;
	}

	(*reg) = daisy & mask;
	hal_cpuDataMemoryBarrier();

	return EOK;
}


static int _imxrt_getIOisel(int isel, char *daisy)
{
	volatile u32 *reg;
	u32 mask;

	reg = _imxrt_IOiselGetReg(isel, &mask);
	if (reg == NULL) {
		return -EINVAL;
	}

	*daisy = (*reg) & mask;

	return EOK;
}


/* SRC */


void _imxrt_resetSlice(unsigned int index)
{
	*(imxrt_common.src + src_ctrl + 8 * index) |= 1u;
	hal_cpuDataMemoryBarrier();

	while ((*(imxrt_common.src + src_stat + 8 * index) & 1u) != 0) {
	}
}


/* CCM */

int _imxrt_setDevClock(int clock, int div, int mux, int mfd, int mfn, int state)
{
	unsigned int t;
	volatile u32 *reg = imxrt_common.ccm + (clock * 0x20);

	if ((clock < pctl_clk_cm7) || (clock > pctl_clk_ccm_clko2)) {
		return -1;
	}

	t = *reg & ~0x01ff07ffu;
	*reg = t | (!state << 24) | ((mfn & 0xf) << 20) | ((mfd & 0xf) << 16) | ((mux & 0x7) << 8) | (div & 0xff);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	return 0;
}


int _imxrt_getDevClock(int clock, int *div, int *mux, int *mfd, int *mfn, int *state)
{
	unsigned int t;
	volatile u32 *reg = imxrt_common.ccm + (clock * 0x20);

	if ((clock < pctl_clk_cm7) || (clock > pctl_clk_ccm_clko2)) {
		return -1;
	}

	t = *reg;

	*div = t & 0xff;
	*mux = (t >> 8) & 0x7;
	*mfd = (t >> 16) & 0xf;
	*mfn = (t >> 20) & 0xf;
	*state = !(t & (1 << 24));

	return 0;
}


int _imxrt_setDirectLPCG(int clock, int state)
{
	u32 t;
	volatile u32 *reg;

	if ((clock < pctl_lpcg_m7) || (clock > pctl_lpcg_uniq_edt_i)) {
		return -EINVAL;
	}

	reg = imxrt_common.ccm + 0x1800 + clock * 0x8;

	t = *reg & ~1u;
	*reg = t | (state & 1);

	hal_cpuDataMemoryBarrier();
	hal_cpuInstrBarrier();

	return EOK;
}


int _imxrt_getDirectLPCG(int clock, int *state)
{
	if ((clock < pctl_lpcg_m7) || (clock > pctl_lpcg_uniq_edt_i)) {
		return -EINVAL;
	}

	*state = *((volatile u32 *)(imxrt_common.ccm + 0x1800 + clock * 0x8)) & 1u;

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
	*reg = (level << 28) | (level << 24) | (level << 20) | (level << 16) | level;

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

	return  0;
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

	return  0;
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
			break;

		case pctl_lpcg:
			if (data->action == pctl_set) {
				if (data->lpcg.op == pctl_lpcg_op_direct) {
					ret = _imxrt_setDirectLPCG(data->lpcg.dev, data->lpcg.state);
				}
				else if (data->lpcg.op == pctl_lpcg_op_level) {
					ret = _imxrt_setLevelLPCG(data->lpcg.dev, data->lpcg.state);
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
			break;

		case pctl_iomux:
			if (data->action == pctl_set) {
				ret = _imxrt_setIOmux(data->iomux.mux, data->iomux.sion, data->iomux.mode);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getIOmux(data->iomux.mux, &data->iomux.sion, &data->iomux.mode);
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
			break;

		case pctl_ioisel:
			if (data->action == pctl_set) {
				ret = _imxrt_setIOisel(data->ioisel.isel, data->ioisel.daisy);
			}
			else if (data->action == pctl_get) {
				ret = _imxrt_getIOisel(data->ioisel.isel, &data->ioisel.daisy);
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
			break;

		case pctl_devcache:
			if (data->action == pctl_set) {
				if (data->devcache.state == 0) {
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
				if ((data->resetSlice.index >= pctl_resetSliceMega) && (data->resetSlice.index <= pctl_resetSliceCM7Mem)) {
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
			break;

		default:
			break;
	}

	hal_spinlockClear(&imxrt_common.pltctlSp, &sc);

	return ret;
}


void _imxrt_platformInit(void)
{
	hal_spinlockCreate(&imxrt_common.pltctlSp, "pltctlSp");
}


extern time_t hal_timerCyc2Us(time_t ticks);
extern time_t hal_timerGetCyc(void);


void testGPIOlatencyConfigure(void)  // #MPUTEST: configure GPIOs
{
	u32 t;
	/* pctl_mux_gpio_ad_XX - GPIO_MUX3 pin XX-1 in ALT5 */
	_imxrt_setIOmux(pctl_mux_gpio_ad_01 + MPUTEST_PIN0, 0, 5);
	_imxrt_setIOpad(pctl_mux_gpio_ad_01 + MPUTEST_PIN0, 1, 0, 0, 0, 0, 0);
	_imxrt_setIOmux(pctl_mux_gpio_ad_01 + MPUTEST_PIN1, 0, 5);
	_imxrt_setIOpad(pctl_mux_gpio_ad_01 + MPUTEST_PIN1, 1, 0, 0, 0, 0, 0);


	/* set up pins of GPIO_MUX3 to CM7 fast GPIO */
#if MPUTEST_PIN1 >= 16
	// GPR43
	*((u32 *)0x400E40AC) |= (1 << (MPUTEST_PIN0 - 16));
#else
	// GPR42
	*((u32 *)0x400E40A8) |= (1 << (MPUTEST_PIN0));
#endif
#if MPUTEST_PIN1 >= 16
	// GPR43
	*((u32 *)0x400E40AC) |= (1 << (MPUTEST_PIN1 - 16));
#else
	// GPR42
	*((u32 *)0x400E40A8) |= (1 << (MPUTEST_PIN1));
#endif

	/* set dir */
	t = *(CM7_GPIO3_BASE + gdir) & ~(1 << MPUTEST_PIN0);
	*(CM7_GPIO3_BASE + gdir) = t | ((!!gpio_out) << MPUTEST_PIN0);
	t = *(CM7_GPIO3_BASE + gdir) & ~(1 << MPUTEST_PIN1);
	*(CM7_GPIO3_BASE + gdir) = t | ((!!gpio_out) << MPUTEST_PIN1);


	MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
	MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);

	hal_consolePrint(ATTR_BOLD, "Delay here should take about 1s\n");
	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 1000000;) {
		__asm__ volatile("nop");
	}
	hal_consolePrint(ATTR_BOLD, "DELAY DONE\n");
}


typedef struct
{
	volatile const u32 CPUID;       /*!< Offset: 0x000 (R/ )  CPUID Base Register */
	volatile u32 ICSR;              /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register */
	volatile u32 VTOR;              /*!< Offset: 0x008 (R/W)  Vector Table Offset Register */
	volatile u32 AIRCR;             /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register */
	volatile u32 SCR;               /*!< Offset: 0x010 (R/W)  System Control Register */
	volatile u32 CCR;               /*!< Offset: 0x014 (R/W)  Configuration Control Register */
	volatile u8 SHPR[12U];          /*!< Offset: 0x018 (R/W)  System Handlers Priority Registers (4-7, 8-11, 12-15) */
	volatile u32 SHCSR;             /*!< Offset: 0x024 (R/W)  System Handler Control and State Register */
	volatile u32 CFSR;              /*!< Offset: 0x028 (R/W)  Configurable Fault Status Register */
	volatile u32 HFSR;              /*!< Offset: 0x02C (R/W)  HardFault Status Register */
	volatile u32 DFSR;              /*!< Offset: 0x030 (R/W)  Debug Fault Status Register */
	volatile u32 MMFAR;             /*!< Offset: 0x034 (R/W)  MemManage Fault Address Register */
	volatile u32 BFAR;              /*!< Offset: 0x038 (R/W)  BusFault Address Register */
	volatile u32 AFSR;              /*!< Offset: 0x03C (R/W)  Auxiliary Fault Status Register */
	volatile const u32 ID_PFR[2U];  /*!< Offset: 0x040 (R/ )  Processor Feature Register */
	volatile const u32 ID_DFR;      /*!< Offset: 0x048 (R/ )  Debug Feature Register */
	volatile const u32 ID_AFR;      /*!< Offset: 0x04C (R/ )  Auxiliary Feature Register */
	volatile const u32 ID_MMFR[4U]; /*!< Offset: 0x050 (R/ )  Memory Model Feature Register */
	volatile const u32 ID_ISAR[5U]; /*!< Offset: 0x060 (R/ )  Instruction Set Attributes Register */
	u32 RESERVED0[1U];
	volatile const u32 CLIDR;  /*!< Offset: 0x078 (R/ )  Cache Level ID register */
	volatile const u32 CTR;    /*!< Offset: 0x07C (R/ )  Cache Type register */
	volatile const u32 CCSIDR; /*!< Offset: 0x080 (R/ )  Cache Size ID Register */
	volatile u32 CSSELR;       /*!< Offset: 0x084 (R/W)  Cache Size Selection Register */
	volatile u32 CPACR;        /*!< Offset: 0x088 (R/W)  Coprocessor Access Control Register */
	u32 RESERVED3[93U];
	volatile u32 STIR; /*!< Offset: 0x200 ( /W)  Software Triggered Interrupt Register */
	u32 RESERVED4[15U];
	volatile const u32 MVFR0; /*!< Offset: 0x240 (R/ )  Media and VFP Feature Register 0 */
	volatile const u32 MVFR1; /*!< Offset: 0x244 (R/ )  Media and VFP Feature Register 1 */
	volatile const u32 MVFR2; /*!< Offset: 0x248 (R/ )  Media and VFP Feature Register 2 */
	u32 RESERVED5[1U];
	volatile u32 ICIALLU; /*!< Offset: 0x250 ( /W)  I-Cache Invalidate All to PoU */
	u32 RESERVED6[1U];
	volatile u32 ICIMVAU;  /*!< Offset: 0x258 ( /W)  I-Cache Invalidate by MVA to PoU */
	volatile u32 DCIMVAC;  /*!< Offset: 0x25C ( /W)  D-Cache Invalidate by MVA to PoC */
	volatile u32 DCISW;    /*!< Offset: 0x260 ( /W)  D-Cache Invalidate by Set-way */
	volatile u32 DCCMVAU;  /*!< Offset: 0x264 ( /W)  D-Cache Clean by MVA to PoU */
	volatile u32 DCCMVAC;  /*!< Offset: 0x268 ( /W)  D-Cache Clean by MVA to PoC */
	volatile u32 DCCSW;    /*!< Offset: 0x26C ( /W)  D-Cache Clean by Set-way */
	volatile u32 DCCIMVAC; /*!< Offset: 0x270 ( /W)  D-Cache Clean and Invalidate by MVA to PoC */
	volatile u32 DCCISW;   /*!< Offset: 0x274 ( /W)  D-Cache Clean and Invalidate by Set-way */
	volatile u32 BPIALL;   /*!< Offset: 0x278 ( /W)  Branch Predictor Invalidate All */
	u32 RESERVED7[5U];
	volatile u32 ITCMCR; /*!< Offset: 0x290 (R/W)  Instruction Tightly-Coupled Memory Control Register */
	volatile u32 DTCMCR; /*!< Offset: 0x294 (R/W)  Data Tightly-Coupled Memory Control Registers */
	volatile u32 AHBPCR; /*!< Offset: 0x298 (R/W)  AHBP Control Register */
	volatile u32 CACR;   /*!< Offset: 0x29C (R/W)  L1 Cache Control Register */
	volatile u32 AHBSCR; /*!< Offset: 0x2A0 (R/W)  AHB Slave Control Register */
	u32 RESERVED8[1U];
	volatile u32 ABFSR; /*!< Offset: 0x2A8 (R/W)  Auxiliary Bus Fault Status Register */
} SCB_Type;

#define SCS_BASE (0xE000E000UL)         /*!< System Control Space Base Address */
#define SCB_BASE (SCS_BASE + 0x0D00UL)  /*!< System Control Block Base Address */
#define SCB      ((SCB_Type *)SCB_BASE) /*!< SCB configuration struct */
void hal_invalICacheAll(void)           // copied from https://github.com/ARM-software/CMSIS_6/blob/671eba9606a5e3db8b6d45d3475880ff820ca456/CMSIS/Core/Include/m-profile/armv7m_cachel1.h#L93
{
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	SCB->ICIALLU = 0UL;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}

enum { scb_cpuid = 0,
	scb_icsr,
	scb_vtor,
	scb_aircr,
	scb_scr,
	scb_ccr,
	scb_shp0,
	scb_shp1,
	scb_shp2,
	scb_shcsr,
	scb_cfsr,
	scb_hfsr,
	scb_dfsr,
	scb_mmfar,
	scb_bfar,
	scb_afsr,
	scb_pfr0,
	scb_pfr1,
	scb_dfr,
	scb_afr,
	scb_mmfr0,
	scb_mmfr1,
	scb_mmfr2,
	scb_mmf3,
	scb_isar0,
	scb_isar1,
	scb_isar2,
	scb_isar3,
	scb_isar4,
	/* reserved */ scb_clidr = 30,
	scb_ctr,
	scb_ccsidr,
	scb_csselr,
	scb_cpacr,
	/* 93 reserved */ scb_stir = 128,
	/* 15 reserved */ scb_mvfr0 = 144,
	scb_mvfr1,
	scb_mvfr2,
	/* reserved */ scb_iciallu = 148,
	/* reserved */ scb_icimvau = 150,
	scb_dcimvac,
	scb_dcisw,
	scb_dccmvau,
	scb_dccmvac,
	scb_dccsw,
	scb_dccimvac,
	scb_dccisw, /* 6 reserved */
	scb_itcmcr = 164,
	scb_dtcmcr,
	scb_ahbpcr,
	scb_cacr,
	scb_ahbscr,
	/* reserved */ scb_abfsr = 170 };
void hal_invalDCacheAll(void)
{
	u32 ccsidr, sets, ways;

	/* select Level 1 data cache */

	volatile u32 *scb = (void *)0xe000ed00;

	*(scb + scb_csselr) = 0;
	hal_cpuDataSyncBarrier();
	ccsidr = *(scb + scb_ccsidr);

	sets = (ccsidr >> 13) & 0x7fff;
	do {
		ways = (ccsidr >> 3) & 0x3ff;
		do {
			*(scb + scb_dcisw) = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
		} while (ways-- != 0U);
	} while (sets-- != 0U);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}

void testGPIOlatency(int cacheopt)  // #MPUTEST: TEST GPIO LATENCY
{
	const int ITER_CNT = 10 * 1000;

	for (int i = 0; i < ITER_CNT; i++) {
		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100;) {
			__asm__ volatile("nop");
		}
		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100;) {
			__asm__ volatile("nop");
		}
		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);

		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 1000;) {
			__asm__ volatile("nop");
		}
		if (cacheopt == 1) {
			hal_invalDCacheAll();
			hal_invalICacheAll();
		}
	}

	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
		__asm__ volatile("nop");
	}

	for (int i = 0; i < ITER_CNT; i++) {
		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100;) {
			__asm__ volatile("nop");
		}
		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100;) {
			__asm__ volatile("nop");
		}
		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);

		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 1000;) {
			__asm__ volatile("nop");
		}
		if (cacheopt == 1) {
			hal_invalDCacheAll();
			hal_invalICacheAll();
		}
	}

	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
		__asm__ volatile("nop");
	}

	int avgTwoCycles = 0;
	int curTwoCycles;
	int avgOnCycles = 0;
	int curOnCycles;
	for (int i = 0; i < ITER_CNT; i++) {
		curTwoCycles = hal_timerGetCyc();
		/* measure single GPIO toggle with second GPIO */
		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
		curTwoCycles = hal_timerGetCyc() - curTwoCycles;
		avgTwoCycles = (avgTwoCycles * i + curTwoCycles) / (i + 1);

		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100;) {
			__asm__ volatile("nop");
		}
		if (cacheopt == 1) {
			hal_invalDCacheAll();
			hal_invalICacheAll();
		}
	}

	char b[200];
	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	lib_sprintf(b, "GPIO latency test (pad toggling) - avg Two PINS ON/OFF: %d cycles (%d us)\n",
			avgTwoCycles, (int)hal_timerCyc2Us(avgTwoCycles));
	hal_consolePrint(ATTR_BOLD, b);

	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
		__asm__ volatile("nop");
	}

	for (int i = 0; i < 100; i++) {
		/* Measure time of 1000 ON/OFF switches */
		curOnCycles = hal_timerGetCyc();
		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
		for (int i = 0; i < ITER_CNT; i++) {
			MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
			MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
		}
		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);

		curOnCycles = hal_timerGetCyc() - curOnCycles;
		avgOnCycles = (avgOnCycles * i + curOnCycles) / (i + 1);

		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 1000;) {
			__asm__ volatile("nop");
		}
		if (cacheopt == 1) {
			hal_invalDCacheAll();
			hal_invalICacheAll();
		}
	}

	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	lib_sprintf(b, "GPIO latency test (pad toggling) - avg 1 PIN x%d times ON/OFF: %d cycles (%d us)\n",
			ITER_CNT, avgOnCycles, (int)hal_timerCyc2Us(avgOnCycles));
	hal_consolePrint(ATTR_BOLD, b);
}


void _imxrt_init(void)
{
	u32 tmp;

	imxrt_common.aips[0] = (void *)0x40000000;
	imxrt_common.aips[1] = (void *)0x40400000;
	imxrt_common.aips[2] = (void *)0x40800000;
	imxrt_common.aips[3] = (void *)0x40c00000;
	imxrt_common.ccm = (void *)0x40cc0000;
	imxrt_common.stk = (void *)0xe000e010;
	imxrt_common.wdog1 = (void *)0x40030000;
	imxrt_common.wdog2 = (void *)0x40034000;
	imxrt_common.rtwdog3 = (void *)0x40038000;
	imxrt_common.rtwdog4 = (void *)0x40c10000;
	imxrt_common.src = (void *)0x40c04000;
	imxrt_common.iomux_snvs = (void *)0x40c94000;
	imxrt_common.iomux_lpsr = (void *)0x40c08000;
	imxrt_common.iomuxc = (void *)0x400e8000;
	imxrt_common.gpr = (void *)0x400e4000;
	imxrt_common.lpsrgpr = (void *)0x40c0c000;

	imxrt_common.cpuclk = 696000000;

	_hal_scsInit();

	/* WDOG1 and WDOG2 can't be disabled once enabled */

	/* Enabling the watchdog and setting the timeout are separate actions controlled by WATCHDOG and
	WATCHDOG_TIMEOUT_MS, so it is possible to e.g. change the timeout if the watchdog was already
	enabled by plo or bootrom, but not enabling it if it was disabled. */

#if defined(WATCHDOG_TIMEOUT_MS)
	/* Set the timeout (always possible) */
	tmp = (*(imxrt_common.wdog1 + wdog_wcr) & ~(0xffu << 8));
	*(imxrt_common.wdog1 + wdog_wcr) = tmp | (((WATCHDOG_TIMEOUT_MS - 500u) / 500u) << 8);
	hal_cpuDataMemoryBarrier();
#endif
#if defined(WATCHDOG)
	/* Enable the watchdog */
	*(imxrt_common.wdog1 + wdog_wcr) |= (1u << 2);
	hal_cpuDataMemoryBarrier();
#endif
#if defined(WATCHDOG_TIMEOUT_MS)
	/* Reload the watchdog with a new timeout value in case it was already enabled by
	bootrom/plo and was running with a different timeout */
	_imxrt_wdgReload();
#endif

	/* Disable WDOG3 and WDOG4 in case plo didn't do this */

	if ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1u << 7)) != 0u) {
		/* WDOG3: Unlock rtwdog update */
		*(imxrt_common.rtwdog3 + rtwdog_cnt) = RTWDOG_UNLOCK_KEY;
		hal_cpuDataMemoryBarrier();
		while ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1u << 11u)) == 0u) {
		}

		/* WDOG3: Disable rtwdog, but allow later reconfiguration without reset */
		*(imxrt_common.rtwdog3 + rtwdog_toval) = 0xffffu;
		tmp = (*(imxrt_common.rtwdog3 + rtwdog_cs) & ~(1u << 7u));
		*(imxrt_common.rtwdog3 + rtwdog_cs) = tmp | (1u << 5u);

		/* WDOG3: Wait until new config takes effect */
		while ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1u << 10u)) == 0u) {
		}

		/* WDOG3: Wait until registers are locked (in case low power mode will be used promptly) */
		while ((*(imxrt_common.rtwdog3 + rtwdog_cs) & (1u << 11)) != 0u) {
		}
	}

	if ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1u << 7)) != 0u) {
		/* WDOG4: Unlock rtwdog update */
		*(imxrt_common.rtwdog4 + rtwdog_cnt) = RTWDOG_UNLOCK_KEY;
		hal_cpuDataMemoryBarrier();
		while ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1u << 11u)) == 0u) {
		}

		/* WDOG4: Disable rtwdog, but allow later reconfiguration without reset */
		*(imxrt_common.rtwdog4 + rtwdog_toval) = 0xffffu;
		tmp = (*(imxrt_common.rtwdog4 + rtwdog_cs) & ~(1u << 7u));
		*(imxrt_common.rtwdog4 + rtwdog_cs) = tmp | (1u << 5u);

		/* WDOG4: Wait until new config takes effect */
		while ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1u << 10u)) == 0u) {
		}

		/* WDOG4: Wait until registers are locked (in case low power mode will be used promptly) */
		while ((*(imxrt_common.rtwdog4 + rtwdog_cs) & (1u << 11)) != 0u) {
		}
	}

	/* Enable system HP timer clock gate, select SYS_PLL3_DIV2 as BUS clk */
	_imxrt_setDevClock(GPT_BUS_CLK, 0, 4, 0, 0, 1);

	/* Enable FPU */
	_hal_scsFPUSet(1);
}
