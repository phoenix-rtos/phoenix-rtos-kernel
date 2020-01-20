/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * iMXRT basic peripherals control functions
 *
 * Copyright 2017, 2019 Phoenix Systems
 * Author: Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "imxrt.h"
#include "interrupts.h"
#include "pmap.h"
#include "../../../include/errno.h"
#include "../../../include/arch/imxrt.h"


struct {
	volatile u32 *gpio[5];
	volatile u32 *aips[4];
	volatile u32 *ccm;
	volatile u32 *ccm_analog;
	volatile u32 *pmu;
	volatile u32 *xtalosc;
	volatile u32 *iomuxc;
	volatile u32 *iomuxgpr;
	volatile u32 *iomuxsnvs;
	volatile u32 *nvic;
	volatile u32 *stk;
	volatile u32 *scb;
	volatile u32 *mpu;
	volatile u16 *wdog1;
	volatile u16 *wdog2;
	volatile u32 *rtwdog;
	volatile u32 *src;

	u32 resetFlags;

	u32 xtaloscFreq;
	u32 cpuclk;

	spinlock_t pltctlSp;
} imxrt_common;


enum { gpio_dr = 0, gpio_gdir, gpio_psr, gpio_icr1, gpio_icr2, gpio_imr, gpio_isr, gpio_edge_sel };


enum { aipstz_mpr = 0, aipstz_opacr = 16, aipstz_opacr1, aipstz_opacr2, aipstz_opacr3, aipstz_opacr4 };


enum { ccm_ccr = 0, /* reserved */ ccm_csr = 2, ccm_ccsr, ccm_cacrr, ccm_cbcdr, ccm_cbcmr, ccm_cscmr1, ccm_cscmr2,
	ccm_cscdr1, ccm_cs1cdr, ccm_cs2cdr, ccm_cdcdr, /* reserved */ ccm_cscdr2 = 14, ccm_cscdr3, /* 2 reserved */
	ccm_cdhipr = 18, /* 2 reserved */ ccm_clpcr = 21, ccm_cisr, ccm_cimr, ccm_ccosr, ccm_cgpr, ccm_ccgr0, ccm_ccgr1,
	ccm_ccgr2, ccm_ccgr3, ccm_ccgr4, ccm_ccgr5, ccm_ccgr6, ccm_ccgr7, ccm_cmeor };


enum { ccm_analog_pll_arm, ccm_analog_pll_arm_set, ccm_analog_pll_arm_clr, ccm_analog_pll_arm_tog,
	ccm_analog_pll_usb1, ccm_analog_pll_usb1_set, ccm_analog_pll_usb1_clr, ccm_analog_pll_usb1_tog,
	ccm_analog_pll_usb2, ccm_analog_pll_usb2_set, ccm_analog_pll_usb2_clr, ccm_analog_pll_usb2_tog,
	ccm_analog_pll_sys, ccm_analog_pll_sys_set, ccm_analog_pll_sys_clr, ccm_analog_pll_sys_tog,
	ccm_analog_pll_sys_ss, /* 3 reserved */ ccm_analog_pll_sys_num = 20, /* 3 reserved */
	ccm_analog_pll_sys_denom = 24, /* 3 reserved */ ccm_analog_pll_audio = 28, ccm_analog_pll_audio_set,
	ccm_analog_pll_audio_clr, ccm_analog_pll_audio_tog, ccm_analog_pll_audio_num, /* 3 reserved */
	ccm_analog_pll_audio_denom = 36, /* 3 reserved */ ccm_analog_pll_video = 40, ccm_analog_pll_video_set,
	ccm_analog_pll_video_clr, ccm_analog_pll_video_tog, ccm_analog_pll_video_num, /* 3 reserved */
	ccm_analog_pll_video_denom = 48, /* 3 reserved */ ccm_analog_pll_enet = 56, ccm_analog_pll_enet_set,
	ccm_analog_pll_enet_clr, ccm_analog_pll_enet_tog, ccm_analog_pfd_480, ccm_analog_pfd_480_set,
	ccm_analog_pfd_480_clr, ccm_analog_pfd_480_tog, ccm_analog_pfd_528, ccm_analog_pfd_528_set,
	ccm_analog_pfd_528_clr, ccm_analog_pfd_528_tog, ccm_analog_misc0 = 84, ccm_analog_misc0_set,
	ccm_analog_misc0_clr, ccm_analog_misc0_tog, ccm_analog_misc1, ccm_analog_misc1_set, ccm_analog_misc1_clr,
	ccm_analog_misc1_tog, ccm_analog_misc2, ccm_analog_misc2_set, ccm_analog_misc2_clr, ccm_analog_misc2_tog };


enum { pmu_reg_1p1 = 0, /* 3 reserved */ pmu_reg_3p0 = 4, /* 3 reserved */ pmu_reg_2p5 = 8, /* 3 reserved */
	pmu_reg_core = 12, /* 3 reserved */ pmu_misc0 = 16, /* 3 reserved */ pmu_misc1 = 20, pmu_misc1_set,
	pmu_misc1_clr, pmu_misc1_tog, pmu_misc2, pmu_misc2_set, pmu_misc2_clr, pmu_misc2_tog };


enum { xtalosc_misc0 = 84, xtalosc_lowpwr_ctrl = 156, xtalosc_lowpwr_ctrl_set, xtalosc_lowpwr_ctrl_clr,
	xtalosc_lowpwr_ctrl_tog, xtalosc_osc_config0 = 168, xtalosc_osc_config0_set, xtalosc_osc_config0_clr,
	xtalosc_osc_config0_tog, xtalosc_osc_config1, xtalosc_osc_config1_set, xtalosc_osc_config1_clr,
	xtalosc_osc_config1_tog, xtalosc_osc_config2, xtalosc_osc_config2_set, xtalosc_osc_config2_clr, xtalosc_osc_config2_tog };


enum { osc_rc = 0, osc_xtal };


enum { stk_ctrl = 0, stk_load, stk_val, stk_calib };


enum { src_scr = 0, src_sbmr1, src_srsr, src_sbmr2 = 7, src_gpr1, src_gpr2, src_gpr3, src_gpr4,
	src_gpr5, src_gpr6, src_gpr7, src_gpr8, src_gpr9, src_gpr10 };


enum { scb_cpuid = 0, scb_icsr, scb_vtor, scb_aircr, scb_scr, scb_ccr, scb_shp0, scb_shp1,
	scb_shp2, scb_shcsr, scb_cfsr, scb_hfsr, scb_dfsr, scb_mmfar, scb_bfar, scb_afsr, scb_pfr0,
	scb_pfr1, scb_dfr, scb_afr, scb_mmfr0, scb_mmfr1, scb_mmfr2, scb_mmf3, scb_isar0, scb_isar1,
	scb_isar2, scb_isar3, scb_isar4, /* reserved */ scb_clidr = 30, scb_ctr, scb_ccsidr, scb_csselr,
	scb_cpacr, /* 93 reserved */ scb_stir = 128, /* 15 reserved */ scb_mvfr0 = 144, scb_mvfr1,
	scb_mvfr2, /* reserved */ scb_iciallu = 148, /* reserved */ scb_icimvau = 150, scb_scimvac,
	scb_dcisw, scb_dccmvau, scb_dccmvac, scb_dccsw, scb_dccimvac, scb_dccisw, /* 6 reserved */
	scb_itcmcr = 164, scb_dtcmcr, scb_ahbpcr, scb_cacr, scb_ahbscr, /* reserved */ scb_abfsr = 170 };

enum { mpu_type, mpu_ctrl, mpu_rnr, mpu_rbar, mpu_rasr, mpu_rbar_a1, mpu_rasr_a1, mpu_rbar_a2, mpu_rasr_a2,
       mpu_rbar_a3, mpu_rasr_a3 };

enum { nvic_iser = 0, nvic_icer = 32, nvic_ispr = 64, nvic_icpr = 96, nvic_iabr = 128,
	nvic_ip = 256, nvic_stir = 896 };


enum { wdog_wcr = 0, wdog_wsr, wdog_wrsr, wdog_wicr, wdog_wmcr };


enum { rtwdog_cs = 0, rtwdog_cnt, rtwdog_total, rtwdog_win };


/* platformctl syscall */


static int _imxrt_isValidDev(int dev)
{
	if (dev < pctl_clk_aips_tz1 || dev > pctl_clk_flexio3)
		return 0;

	return 1;
}


static int _imxrt_getDevClock(int dev, unsigned int *state)
{
	int ccgr, flag;

	if (!_imxrt_isValidDev(dev))
		return -EINVAL;

	ccgr = dev / 16;
	flag = 3 << (2 * (dev % 16));

	*state = (*(imxrt_common.ccm + ccm_ccgr0 + ccgr) & flag) >> (2 * (dev % 16));

	return EOK;
}


static int _imxrt_setDevClock(int dev, unsigned int state)
{
	int ccgr, flag, mask;
	u32 t;

	if (!_imxrt_isValidDev(dev))
		return -EINVAL;

	ccgr = dev / 16;
	flag = (state & 3) << (2 * (dev % 16));
	mask = 3 << (2 * (dev % 16));

	t = *(imxrt_common.ccm + ccm_ccgr0 + ccgr) & ~mask;
	*(imxrt_common.ccm + ccm_ccgr0 + ccgr) = t | flag;

	return EOK;
}


static int _imxrt_checkIOgprArg(int field, unsigned int *mask)
{
	if (field < pctl_gpr_sai1_mclk1_sel || field > pctl_gpr_sip_test_mux_qspi_sip_en)
		return -EINVAL;

	switch (field) {
		case pctl_gpr_sai1_mclk3_sel:
		case pctl_gpr_sai2_mclk3_sel:
		case pctl_gpr_sai3_mclk3_sel:
		case pctl_gpr_m7_apc_ac_r0_ctrl:
		case pctl_gpr_m7_apc_ac_r1_ctrl:
		case pctl_gpr_m7_apc_ac_r2_ctrl:
		case pctl_gpr_m7_apc_ac_r3_ctrl:
			(*mask) = 0x3;
			break;

		case pctl_gpr_sai1_mclk1_sel:
		case pctl_gpr_sai1_mclk2_sel:
			(*mask) = 0x7;
			break;

		case pctl_gpr_ocram_ctl:
		case pctl_gpr_ocram2_ctl:
		case pctl_gpr_ocram_status:
		case pctl_gpr_ocram2_status:
		case pctl_gpr_bee_de_rx_en:
		case pctl_gpr_cm7_cfgitcmsz:
		case pctl_gpr_cm7_cfgdtcmsz:
			(*mask) = 0xf;
			break;

		case pctl_gpr_ocram_tz_addr:
		case pctl_gpr_lock_ocram_tz_addr:
		case pctl_gpr_ocram2_tz_addr:
		case pctl_gpr_lock_ocram2_tz_addr:
			(*mask) = 0x7f;
			break;

		case pctl_gpr_mqs_clk_div:
		case pctl_gpr_sip_test_mux_qspi_sip_sel:
			(*mask) = 0xff;
			break;

		case pctl_gpr_flexspi_remap_addr_start:
		case pctl_gpr_flexspi_remap_addr_end:
		case pctl_gpr_flexspi_remap_addr_offset:
			(*mask) = 0xfffff;

		case pctl_gpr_m7_apc_ac_r0_bot:
		case pctl_gpr_m7_apc_ac_r0_top:
		case pctl_gpr_m7_apc_ac_r1_bot:
		case pctl_gpr_m7_apc_ac_r1_top:
		case pctl_gpr_m7_apc_ac_r2_bot:
		case pctl_gpr_m7_apc_ac_r2_top:
		case pctl_gpr_m7_apc_ac_r3_bot:
		case pctl_gpr_m7_apc_ac_r3_top:
			(*mask) = 0x1fffffff;
			break;

		case pctl_gpr_flexram_bank_cfg:
		case pctl_gpr_gpio_mux1_gpio_sel:
		case pctl_gpr_gpio_mux2_gpio_sel:
		case pctl_gpr_gpio_mux3_gpio_sel:
		case pctl_gpr_gpio_mux4_gpio_sel:
			(*mask) = 0xffffffffUL;
			break;

		default:
			(*mask) = 1;
			break;
	}

	return EOK;
}


static int _imxrt_setIOgpr(int field, unsigned int val)
{
	unsigned int mask, t;
	int err;

	if ((err = _imxrt_checkIOgprArg(field, &mask)) != EOK)
		return err;

	t = *(imxrt_common.iomuxgpr+ (field >> 5)) & ~(mask << (field & 0x1f));
	*(imxrt_common.iomuxgpr + (field >> 5)) = t | (val & mask) << (field & 0x1f);

	return EOK;
}


static int _imxrt_getIOgpr(int field, unsigned int *val)
{
	unsigned int mask;
	int err;

	if ((err = _imxrt_checkIOgprArg(field, &mask)) != EOK)
		return err;

	*val = (*(imxrt_common.iomuxgpr + (field >> 5)) >> (field & 0x1f)) & mask;

	return EOK;
}


static volatile u32 *_imxrt_IOmuxGetReg(int mux)
{
	if (mux < pctl_mux_gpio_emc_00 || mux > pctl_mux_snvs_pmic_stby_req)
		return NULL;

	if (mux >= pctl_mux_snvs_wakeup)
		return imxrt_common.iomuxsnvs + (mux - pctl_mux_snvs_wakeup);

	return imxrt_common.iomuxc + mux + 5;
}


int _imxrt_setIOmux(int mux, char sion, char mode)
{
	volatile u32 *reg;

	if ((reg = _imxrt_IOmuxGetReg(mux)) == NULL)
		return -EINVAL;

	(*reg) = (!!sion << 4) | (mode & 0xf);

	return EOK;
}


static int _imxrt_getIOmux(int mux, char *sion, char *mode)
{
	u32 t;
	volatile u32 *reg;

	if ((reg = _imxrt_IOmuxGetReg(mux)) == NULL)
		return -EINVAL;

	t = (*reg);
	*sion = !!(t & (1 << 4));
	*mode = t & 0xf;

	return EOK;
}


static volatile u32 *_imxrt_IOpadGetReg(int pad)
{
	if (pad < pctl_pad_gpio_emc_00 || pad > pctl_pad_snvs_pmic_stby_req)
		return NULL;

	if (pad >= pctl_pad_snvs_test_mode)
		return imxrt_common.iomuxsnvs + 3 + (pad - pctl_pad_snvs_test_mode);

	if (pad >= pctl_pad_gpio_spi_b0_00)
		return imxrt_common.iomuxc + 429 + (pad - pctl_pad_gpio_spi_b0_00);

	return imxrt_common.iomuxc + 129 + pad;
}


int _imxrt_setIOpad(int pad, char hys, char pus, char pue, char pke, char ode, char speed, char dse, char sre)
{
	u32 t;
	volatile u32 *reg;

	if ((reg = _imxrt_IOpadGetReg(pad)) == NULL)
		return -EINVAL;

	t = (!!hys << 16) | ((pus & 0x3) << 14) | (!!pue << 13) | (!!pke << 12);
	t |= (!!ode << 11) | ((speed & 0x3) << 6) | ((dse & 0x7) << 3) | !!sre;
	(*reg) = t;

	return EOK;
}


static int _imxrt_getIOpad(int pad, char *hys, char *pus, char *pue, char *pke, char *ode, char *speed, char *dse, char *sre)
{
	u32 t;
	volatile u32 *reg;

	if ((reg = _imxrt_IOpadGetReg(pad)) == NULL)
		return -EINVAL;

	t = (*reg);

	*hys = (t >> 16) & 0x1;
	*pus = (t >> 14) & 0x3;
	*pue = (t >> 13) & 0x1;
	*pke = (t >> 12) & 0x1;
	*ode = (t >> 11) & 0x1;
	*speed = (t >> 6) & 0x3;
	*dse = (t >> 3) & 0x7;
	*sre = t & 0x1;

	return EOK;
}


static volatile u32 *_imxrt_IOiselGetReg(int isel, u32 *mask)
{
	if (isel < pctl_isel_anatop_usb_otg1_id || isel > pctl_isel_canfd_ipp_ind_canrx)
		return NULL;

	switch (isel) {
		case pctl_isel_ccm_pmic_ready:
		case pctl_isel_csi_hsync:
		case pctl_isel_csi_vsync:
		case pctl_isel_enet_mdio:
		case pctl_isel_enet0_timer:
		case pctl_isel_flexcan1_rx:
		case pctl_isel_flexcan2_rx:
		case pctl_isel_flexpwm1_pwma3:
		case pctl_isel_flexpwm1_pwmb3:
		case pctl_isel_flexpwm2_pwma3:
		case pctl_isel_flexpwm2_pwmb3:
		case pctl_isel_lpi2c3_scl:
		case pctl_isel_lpi2c3_sda:
		case pctl_isel_lpuart3_rx:
		case pctl_isel_lpuart3_tx:
		case pctl_isel_lpuart4_rx:
		case pctl_isel_lpuart4_tx:
		case pctl_isel_lpuart8_rx:
		case pctl_isel_lpuart8_tx:
		case pctl_isel_qtimer3_timer0:
		case pctl_isel_qtimer3_timer1:
		case pctl_isel_qtimer3_timer2:
		case pctl_isel_qtimer3_timer3:
		case pctl_isel_sai1_mclk2:
		case pctl_isel_sa1_rx_bclk:
		case pctl_isel_sai1_rx_data0:
		case pctl_isel_sai1_rx_sync:
		case pctl_isel_sai1_tx_bclk:
		case pctl_isel_sai1_tx_sync:
		case pctl_isel_usdhc1_cd_b:
		case pctl_isel_usdhc1_wp:
		case pctl_isel_xbar1_in17:
		case pctl_isel_enet2_ipg_clk_rmii:
		case pctl_isel_enet2_ipp_ind_mac0_rxdata:
		case pctl_isel_enet2_ipp_ind_mac0_rxen:
		case pctl_isel_enet2_ipp_ind_mac0_rxerr:
		case pctl_isel_enet2_ipp_ind_mac0_txclk:
		case pctl_isel_semc_i_ipp_ind_dqs4:
		case pctl_isel_canfd_ipp_ind_canrx:
			(*mask) = 0x3;
			break;

		default:
			(*mask) = 0x1;
			break;
	}

	if (isel >= pctl_isel_enet2_ipg_clk_rmii)
		return imxrt_common.iomuxc + 451 + (isel - pctl_isel_enet2_ipg_clk_rmii);

	return imxrt_common.iomuxc + 253 + isel;
}


int _imxrt_setIOisel(int isel, char daisy)
{
	volatile u32 *reg;
	u32 mask;

	if ((reg = _imxrt_IOiselGetReg(isel, &mask)) == NULL)
		return -EINVAL;

	(*reg) = daisy & mask;

	return EOK;
}


static int _imxrt_getIOisel(int isel, char *daisy)
{
	volatile u32 *reg;
	u32 mask;

	if ((reg = _imxrt_IOiselGetReg(isel, &mask)) == NULL)
		return -EINVAL;

	*daisy = (*reg) & mask;

	return EOK;
}


static void _imxrt_reboot(void)
{
	/* TODO */
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	int ret = -EINVAL;

	hal_spinlockSet(&imxrt_common.pltctlSp);

	switch (data->type) {
	case pctl_devclock:
		if (data->action == pctl_set)
			ret = _imxrt_setDevClock(data->devclock.dev, data->devclock.state);
		else if (data->action == pctl_get)
			ret = _imxrt_getDevClock(data->devclock.dev, &data->devclock.state);
		break;

	case pctl_iogpr:
		if (data->action == pctl_set)
			ret = _imxrt_setIOgpr(data->iogpr.field, data->iogpr.val);
		else if (data->action == pctl_get)
			ret = _imxrt_getIOgpr(data->iogpr.field, &data->iogpr.val);
		break;

	case pctl_iomux:
		if (data->action == pctl_set)
			ret = _imxrt_setIOmux(data->iomux.mux, data->iomux.sion, data->iomux.mode);
		else if (data->action == pctl_get)
			ret = _imxrt_getIOmux(data->iomux.mux, &data->iomux.sion, &data->iomux.mode);
		break;

	case pctl_iopad:
		if (data->action == pctl_set)
			ret = _imxrt_setIOpad(data->iopad.pad, data->iopad.hys, data->iopad.pus, data->iopad.pue,
				data->iopad.pke, data->iopad.ode, data->iopad.speed, data->iopad.dse, data->iopad.sre);
		else if (data->action == pctl_get)
			ret = _imxrt_getIOpad(data->iopad.pad, &data->iopad.hys, &data->iopad.pus, &data->iopad.pue,
				&data->iopad.pke, &data->iopad.ode, &data->iopad.speed, &data->iopad.dse, &data->iopad.sre);
		break;

	case pctl_ioisel:
		if (data->action == pctl_set)
			ret = _imxrt_setIOisel(data->ioisel.isel, data->ioisel.daisy);
		else if (data->action == pctl_get)
			ret = _imxrt_getIOisel(data->ioisel.isel, &data->ioisel.daisy);
		break;

	case pctl_reboot:
		if (data->action == pctl_set) {
			if (data->reboot.magic == PCTL_REBOOT_MAGIC)
				_imxrt_reboot();
		}
		else if (data->action == pctl_get) {
			data->reboot.reason = imxrt_common.resetFlags;
			ret = EOK;
		}
		break;

	case pctl_devcache:
		if (data->action == pctl_set) {
			if(data->devcache.state == 0) {
				_imxrt_disableDCache();
				_imxrt_disableICache();
			}
			else {
				_imxrt_enableDCache();
				_imxrt_enableICache();
			}

			ret = EOK;
		}
		break;

	default:
		break;
	}

	hal_spinlockClear(&imxrt_common.pltctlSp);

	return ret;
}


/* CCM (Clock Controller Module) */


static u32 _imxrt_ccmGetPeriphClkFreq(void)
{
	u32 freq;

	/* Periph_clk2_clk ---> Periph_clk */
	if (*(imxrt_common.ccm + ccm_cbcdr) & (1 << 25)) {
		switch ((*(imxrt_common.ccm + ccm_cbcmr) >> 12) & 0x3) {
			/* Pll3_sw_clk ---> Periph_clk2_clk ---> Periph_clk */
			case 0x0:
				freq = _imxrt_ccmGetPllFreq(clk_pll_usb1);
				break;

			/* Osc_clk ---> Periph_clk2_clk ---> Periph_clk */
			case 0x1:
				freq = imxrt_common.xtaloscFreq;
				break;

			default:
				freq = 0;
				break;
		}

		freq /= ((*(imxrt_common.ccm + ccm_cbcdr) >> 27) & 0x7) + 1;
	}
	else { /* Pre_Periph_clk ---> Periph_clk */
		switch ((*(imxrt_common.ccm + ccm_cbcmr) >> 18) * 0x3) {
			/* PLL2 ---> Pre_Periph_clk ---> Periph_clk */
			case 0x0:
				freq = _imxrt_ccmGetPllFreq(clk_pll_sys);
				break;

			/* PLL2 PFD2 ---> Pre_Periph_clk ---> Periph_clk */
			case 0x1:
				freq = _imxrt_ccmGetSysPfdFreq(clk_pfd2);
				break;

			/* PLL2 PFD0 ---> Pre_Periph_clk ---> Periph_clk */
			case 0x2:
				freq = _imxrt_ccmGetSysPfdFreq(clk_pfd0);
				break;

			/* PLL1 divided(/2) ---> Pre_Periph_clk ---> Periph_clk */
			case 0x3:
				freq = _imxrt_ccmGetPllFreq(clk_pll_arm) / ((*(imxrt_common.ccm + ccm_cacrr) & 0x7) + 1);
				break;

			default:
				freq = 0;
				break;
		}
	}

	return freq;
}


void _imxrt_ccmInitExterlnalClk(void)
{
	/* Power up */
	*(imxrt_common.ccm_analog + ccm_analog_misc0_clr) = 1 << 30;
	while (!(*(imxrt_common.xtalosc + xtalosc_lowpwr_ctrl) & (1 << 16)));

	/* Detect frequency */
	*(imxrt_common.ccm_analog + ccm_analog_misc0_set) = 1 << 16;
	while (!(*(imxrt_common.ccm_analog + ccm_analog_misc0) & (1 << 15)));

	*(imxrt_common.ccm_analog + ccm_analog_misc0_clr) = 1 << 16;
}


void _imxrt_ccmDeinitExternalClk(void)
{
	/* Power down */
	*(imxrt_common.ccm_analog + ccm_analog_misc0_set) = 1 << 30;
}


void _imxrt_ccmSwitchOsc(int osc)
{
	if (osc == osc_rc)
		*(imxrt_common.xtalosc + xtalosc_lowpwr_ctrl_set) = 1 << 4;
	else
		*(imxrt_common.xtalosc + xtalosc_lowpwr_ctrl_clr) = 1 << 4;
}


void _imxrt_ccmInitRcOsc24M(void)
{
	*(imxrt_common.xtalosc + xtalosc_lowpwr_ctrl_set) = 1;
}


void _imxrt_ccmDeinitRcOsc24M(void)
{
	*(imxrt_common.xtalosc + xtalosc_lowpwr_ctrl_clr) = 1;
}


u32 _imxrt_ccmGetFreq(int name)
{
	u32 freq;

	switch (name) {
		/* Periph_clk ---> AHB Clock */
		case clk_cpu:
		case clk_ahb:
			freq = _imxrt_ccmGetPeriphClkFreq() / (((*(imxrt_common.ccm + ccm_cbcdr) > 10) & 0x7) + 1);
			break;

		case clk_semc:
			/* SEMC alternative clock ---> SEMC Clock */
			if (*(imxrt_common.ccm + ccm_cbcdr) & (1 << 6)) {
				/* PLL3 PFD1 ---> SEMC alternative clock ---> SEMC Clock */
				if (*(imxrt_common.ccm + ccm_cbcdr) & 0x7)
					freq = _imxrt_ccmGetUsb1PfdFreq(clk_pfd1);
				/* PLL2 PFD2 ---> SEMC alternative clock ---> SEMC Clock */
				else
					freq = _imxrt_ccmGetSysPfdFreq(clk_pfd2);
			}
			/* Periph_clk ---> SEMC Clock */
			else {
				freq = _imxrt_ccmGetPeriphClkFreq();
			}

			freq /= ((*(imxrt_common.ccm + ccm_cbcdr) >> 16) & 0x7) + 1;
			break;

		case clk_ipg:
			/* Periph_clk ---> AHB Clock ---> IPG Clock */
			freq = _imxrt_ccmGetPeriphClkFreq() / (((*(imxrt_common.ccm + ccm_cbcdr) >> 10) & 0x7) + 1);
			freq /= ((*(imxrt_common.ccm + ccm_cbcdr) >> 8) & 0x3) + 1;
			break;

		case clk_osc:
			freq = _imxrt_ccmGetOscFreq();
			break;
		case clk_rtc:
			freq = 32768;
			break;
		case clk_armpll:
			freq = _imxrt_ccmGetPllFreq(clk_pll_arm);
			break;
		case clk_usb1pll:
			freq = _imxrt_ccmGetPllFreq(clk_pll_usb1);
			break;
		case clk_usb1pfd0:
			freq = _imxrt_ccmGetUsb1PfdFreq(clk_pfd0);
			break;
		case clk_usb1pfd1:
			freq = _imxrt_ccmGetUsb1PfdFreq(clk_pfd1);
			break;
		case clk_usb1pfd2:
			freq = _imxrt_ccmGetUsb1PfdFreq(clk_pfd2);
			break;
		case clk_usb1pfd3:
			freq = _imxrt_ccmGetUsb1PfdFreq(clk_pfd3);
			break;
		case clk_usb2pll:
			freq = _imxrt_ccmGetPllFreq(clk_pll_usb2);
			break;
		case clk_syspll:
			freq = _imxrt_ccmGetPllFreq(clk_pll_sys);
			break;
		case clk_syspdf0:
			freq = _imxrt_ccmGetSysPfdFreq(clk_pfd0);
			break;
		case clk_syspdf1:
			freq = _imxrt_ccmGetSysPfdFreq(clk_pfd1);
			break;
		case clk_syspdf2:
			freq = _imxrt_ccmGetSysPfdFreq(clk_pfd2);
			break;
		case clk_syspdf3:
			freq = _imxrt_ccmGetSysPfdFreq(clk_pfd3);
			break;
		case clk_enetpll0:
			freq = _imxrt_ccmGetPllFreq(clk_pll_enet0);
			break;
		case clk_enetpll1:
			freq = _imxrt_ccmGetPllFreq(clk_pll_enet1);
			break;
		case clk_enetpll2:
			freq = _imxrt_ccmGetPllFreq(clk_pll_enet2);
			break;
		case clk_audiopll:
			freq = _imxrt_ccmGetPllFreq(clk_pll_audio);
			break;
		case clk_videopll:
			freq = _imxrt_ccmGetPllFreq(clk_pll_video);
			break;
		default:
			freq = 0U;
			break;
	}

	return freq;
}


u32 _imxrt_ccmGetOscFreq(void)
{
	return imxrt_common.xtaloscFreq;
}


void _imxrt_ccmSetOscFreq(u32 freq)
{
	imxrt_common.xtaloscFreq = freq;
}


void _imxrt_ccmInitArmPll(u32 div)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_arm) = (1 << 13) | (div & 0x7f);

	while (!(*(imxrt_common.ccm_analog + ccm_analog_pll_arm) & (1 << 31)));
}


void _imxrt_ccmDeinitArmPll(void)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_arm) = 1 << 12;
}


void _imxrt_ccmInitSysPll(u8 div)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_sys) =  (1 << 13) | (div & 1);

	while (!(*(imxrt_common.ccm_analog + ccm_analog_pll_sys) & (1 << 31)));
}


void _imxrt_ccmDeinitSysPll(void)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_sys) = 1 << 12;
}


void _imxrt_ccmInitUsb1Pll(u8 div)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_usb1) = (1 << 13) | (1 << 12) | (1 << 6) | (div & 0x3);

	while (!(*(imxrt_common.ccm_analog + ccm_analog_pll_usb1) & (1 << 31)));
}


void _imxrt_ccmDeinitUsb1Pll(void)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_usb1) = 0;
}


void _imxrt_ccmInitUsb2Pll(u8 div)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_usb2) = (1 << 13) | (1 << 12) | (1 << 6) | (div & 0x3);

	while (!(*(imxrt_common.ccm_analog + ccm_analog_pll_usb2) & (1 << 31)));
}


void _imxrt_ccmDeinitUsb2Pll(void)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_usb2) = 0;
}


void _imxrt_ccmInitAudioPll(u8 loopdiv, u8 postdiv, u32 num, u32 denom)
{
	u32 pllAudio;

	*(imxrt_common.ccm_analog + ccm_analog_pll_audio_num) = num & 0x3fffffff;
	*(imxrt_common.ccm_analog + ccm_analog_pll_audio_denom) = denom & 0x3fffffff;

	pllAudio = (1 << 13) | (loopdiv & 0x7f);

	switch (postdiv) {
		case 16:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_set) = (1 << 23) | (1 << 15);
			break;

		case 8:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_set) = (1 << 23) | (1 << 15);
			pllAudio |= 1 << 19;
			break;

		case 4:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_set) = (1 << 23) | (1 << 15);
			pllAudio |= 1 << 20;
			break;

		case 2:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_clr) = (1 << 23) | (1 << 15);
			pllAudio |= 1 << 19;
			break;

		default:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_clr) = (1 << 23) | (1 << 15);
			pllAudio |= 1 << 20;
			break;
	}

	*(imxrt_common.ccm_analog + ccm_analog_pll_audio) = pllAudio;

	while (!(*(imxrt_common.ccm_analog + ccm_analog_pll_audio) & (1 << 31)));
}


void _imxrt_ccmDeinitAudioPll(void)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_audio) = 1 << 12;
}


void _imxrt_ccmInitVideoPll(u8 loopdiv, u8 postdiv, u32 num, u32 denom)
{
	u32 pllVideo;

	*(imxrt_common.ccm_analog + ccm_analog_pll_video_num) = num & 0x3fffffff;
	*(imxrt_common.ccm_analog + ccm_analog_pll_video_denom) = denom & 0x3fffffff;

	pllVideo = (1 << 13) | (loopdiv & 0x7f);

	switch (postdiv) {
		case 16:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_set) = (3 << 30);
			break;

		case 8:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_set) = (3 << 30);
			pllVideo |= 1 << 19;
			break;

		case 4:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_set) = (3 << 30);
			pllVideo |= 1 << 20;
			break;

		case 2:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_clr) = (3 << 30);
			pllVideo |= 1 << 19;
			break;

		default:
			*(imxrt_common.ccm_analog + ccm_analog_misc2_clr) = (3 << 30);
			pllVideo |= 1 << 20;
			break;
	}

	*(imxrt_common.ccm_analog + ccm_analog_pll_video) = pllVideo;

	while (!(*(imxrt_common.ccm_analog + ccm_analog_pll_video) & (1 << 31)));
}


void _imxrt_ccmDeinitVideoPll(void)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_video) = 1 << 12;
}


void _imxrt_ccmInitEnetPll(u8 enclk0, u8 enclk1, u8 enclk2, u8 div0, u8 div1)
{
	u32 enet_pll = ((div1 & 0x3) << 2) | (div0 & 0x3);

	if (enclk0)
		enet_pll |= 1 << 12;

	if (enclk1)
		enet_pll |= 1 << 20;

	if (enclk2)
		enet_pll |= 1 << 21;

	*(imxrt_common.ccm_analog + ccm_analog_pll_enet) = enet_pll;

	while (!(*(imxrt_common.ccm_analog + ccm_analog_pll_enet) & (1 << 31)));
}


void _imxrt_ccmDeinitEnetPll(void)
{
	*(imxrt_common.ccm_analog + ccm_analog_pll_enet) = 1 << 12;
}


u32 _imxrt_ccmGetPllFreq(int pll)
{
	u32 freq, divSel;
	u64 tmp;

	switch (pll) {
		case clk_pll_arm:
			freq = ((_imxrt_ccmGetOscFreq() * (*(imxrt_common.ccm_analog + ccm_analog_pll_arm) & 0x7f)) >> 1);
			break;

		case clk_pll_sys:
			freq = _imxrt_ccmGetOscFreq();

			/* PLL output frequency = Fref * (DIV_SELECT + NUM/DENOM). */
			tmp = ((u64)freq * (u64)*(imxrt_common.ccm_analog + ccm_analog_pll_sys_num)) / (u64)*(imxrt_common.ccm_analog + ccm_analog_pll_sys_denom);

			if (*(imxrt_common.ccm_analog + ccm_analog_pll_sys) & 1)
				freq *= 22;
			else
				freq *= 20;

			freq += (u32)tmp;
			break;

		case clk_pll_usb1:
			freq = _imxrt_ccmGetOscFreq() * ((*(imxrt_common.ccm_analog + ccm_analog_pll_usb1) & 0x3) ? 22 : 20);
			break;

		case clk_pll_audio:
			freq = _imxrt_ccmGetOscFreq();

			divSel = *(imxrt_common.ccm_analog + ccm_analog_pll_audio) & 0x7f;
			tmp = ((u64)freq * (u64)*(imxrt_common.ccm_analog + ccm_analog_pll_audio_num)) / (u64)*(imxrt_common.ccm_analog + ccm_analog_pll_audio_denom);
			freq = freq * divSel + (u32)tmp;

			switch ((*(imxrt_common.ccm_analog + ccm_analog_pll_audio) >> 19) & 0x3) {
				case 0:
					freq >>= 2;
					break;

				case 1:
					freq >>= 1;

				default:
					break;
			}

			if (*(imxrt_common.ccm_analog + ccm_analog_misc2) & (1 << 15)) {
				if (*(imxrt_common.ccm_analog + ccm_analog_misc2) & (1 << 31))
					freq >>= 2;
				else
					freq >>= 1;
			}
			break;

		case clk_pll_video:
			freq = _imxrt_ccmGetOscFreq();

			divSel = *(imxrt_common.ccm_analog + ccm_analog_pll_video) & 0x7F;

			tmp = ((u64)freq * (u64)*(imxrt_common.ccm_analog + ccm_analog_pll_video_num)) / (u64)*(imxrt_common.ccm_analog + ccm_analog_pll_video_denom);

			freq = freq * divSel + (u32)tmp;

			switch ((*(imxrt_common.ccm_analog + ccm_analog_pll_video) >> 19) & 0x3) {
				case 0:
					freq >>= 2;
					break;

				case 1:
					freq >>= 1;
					break;

				default:
					break;
			}

			if (*(imxrt_common.ccm_analog + ccm_analog_misc2) & (1 << 30)) {
				if (*(imxrt_common.ccm_analog + ccm_analog_misc2) & (1 << 31))
					freq >>= 2;
				else
					freq >>= 1;
			}
			break;

		case clk_pll_enet0:
			divSel = *(imxrt_common.ccm_analog + ccm_analog_pll_enet) & 0x3;

			switch (divSel) {
				case 0:
					freq = 25000000;
					break;

				case 1:
					freq = 50000000;
					break;

				case 2:
					freq = 100000000;
					break;

				default:
					freq = 125000000;
					break;
			}
			break;

		case clk_pll_enet1:
			divSel = *(imxrt_common.ccm_analog + ccm_analog_pll_enet) & (0x3 << 2);

			switch (divSel) {
				case 0:
					freq = 25000000;
					break;

				case 1:
					freq = 50000000;
					break;

				case 2:
					freq = 100000000;
					break;

				default:
					freq = 125000000;
					break;
			}
			break;

		case clk_pll_enet2:
			/* ref_enetpll2 if fixed at 25MHz. */
			freq = 25000000;
			break;

		case clk_pll_usb2:
			freq = _imxrt_ccmGetOscFreq() * ((*(imxrt_common.ccm_analog + ccm_analog_pll_usb2) & 0x3) ? 22 : 20);
			break;

		default:
			freq = 0;
			break;
	}

	return freq;
}


void _imxrt_ccmInitSysPfd(int pfd, u8 pfdFrac)
{
	u32 pfd528 = *(imxrt_common.ccm_analog + ccm_analog_pfd_528) & ~(0xbf << (pfd << 3));

	*(imxrt_common.ccm_analog + ccm_analog_pfd_528) = pfd528 | ((1 << 7) << (pfd << 3));
	*(imxrt_common.ccm_analog + ccm_analog_pfd_528) = pfd528 | (((u32)pfdFrac & 0x3f) << (pfd << 3));
}


void _imxrt_ccmDeinitSysPfd(int pfd)
{
	*(imxrt_common.ccm_analog + ccm_analog_pfd_528) |= (1 << 7) << (pfd << 3);
}


void _imxrt_ccmInitUsb1Pfd(int pfd, u8 pfdFrac)
{
	u32 pfd480 = *(imxrt_common.ccm_analog + ccm_analog_pfd_480) & ~(0xbf << (pfd << 3));

	*(imxrt_common.ccm_analog + ccm_analog_pfd_480) = pfd480 | ((1 << 7) << (pfd << 3));
	*(imxrt_common.ccm_analog + ccm_analog_pfd_480) = pfd480 | (((u32)pfdFrac & 0x3f) << (pfd << 3));
}


void _imxrt_ccmDeinitUsb1Pfd(int pfd)
{
	*(imxrt_common.ccm_analog + ccm_analog_pfd_480) |= (1 << 7) << (pfd << 3);
}


u32 _imxrt_ccmGetSysPfdFreq(int pfd)
{
	u32 freq = _imxrt_ccmGetPllFreq(clk_pll_sys);

	switch (pfd) {
		case clk_pfd0:
			freq /= *(imxrt_common.ccm_analog + ccm_analog_pfd_528) & 0x3f;
			break;

		case clk_pfd1:
			freq /= (*(imxrt_common.ccm_analog + ccm_analog_pfd_528) >> 8) & 0x3f;
			break;

		case clk_pfd2:
			freq /= (*(imxrt_common.ccm_analog + ccm_analog_pfd_528) >> 16) & 0x3f;
			break;

		case clk_pfd3:
			freq /= (*(imxrt_common.ccm_analog + ccm_analog_pfd_528) >> 24) & 0x3f;
			break;

		default:
			freq = 0;
			break;
	}

	return freq * 18;
}


u32 _imxrt_ccmGetUsb1PfdFreq(int pfd)
{
	u32 freq = _imxrt_ccmGetPllFreq(clk_pll_usb1);

	switch (pfd)
	{
		case clk_pfd0:
			freq /= *(imxrt_common.ccm_analog + ccm_analog_pfd_480) & 0x3f;
			break;

		case clk_pfd1:
			freq /= (*(imxrt_common.ccm_analog + ccm_analog_pfd_480) >> 8) & 0x3f;
			break;

		case clk_pfd2:
			freq /= (*(imxrt_common.ccm_analog + ccm_analog_pfd_480) >> 16) & 0x3f;
			break;

		case clk_pfd3:
			freq /= (*(imxrt_common.ccm_analog + ccm_analog_pfd_480) >> 24) & 0x3f;
			break;

		default:
			freq = 0U;
			break;
	}

	return freq * 18;
}


void _imxrt_ccmSetMux(int mux, u32 val)
{
	switch (mux) {
		case clk_mux_pll3:
			*(imxrt_common.ccm + ccm_ccsr) = (*(imxrt_common.ccm + ccm_ccsr) & ~1) | (val & 1);
			break;

		case clk_mux_periph:
			*(imxrt_common.ccm + ccm_cbcdr) = (*(imxrt_common.ccm + ccm_cbcdr) & ~(1 << 25)) | ((val & 1) << 25);
			while (*(imxrt_common.ccm + ccm_cdhipr) & (1 << 5));
			break;

		case clk_mux_semcAlt:
			*(imxrt_common.ccm + ccm_cbcdr) = (*(imxrt_common.ccm + ccm_cbcdr) & ~(1 << 7)) | ((val & 1) << 7);
			break;

		case clk_mux_semc:
			*(imxrt_common.ccm + ccm_cbcdr) = (*(imxrt_common.ccm + ccm_cbcdr) & ~(1 << 6)) | ((val & 1) << 6);
			break;

		case clk_mux_prePeriph:
			*(imxrt_common.ccm + ccm_cbcmr) = (*(imxrt_common.ccm + ccm_cbcmr) & ~(0x3 << 18)) | ((val & 0x3) << 18);
			break;

		case clk_mux_trace:
			*(imxrt_common.ccm + ccm_cbcmr) = (*(imxrt_common.ccm + ccm_cbcmr) & ~(0x3 << 14)) | ((val & 0x3) << 14);
			break;

		case clk_mux_periphclk2:
			*(imxrt_common.ccm + ccm_cbcmr) = (*(imxrt_common.ccm + ccm_cbcmr) & ~(0x3 << 12)) | ((val & 0x3) << 12);
			break;

		case clk_mux_lpspi:
			*(imxrt_common.ccm + ccm_cbcmr) = (*(imxrt_common.ccm + ccm_cbcmr) & ~(0x3 << 4)) | ((val & 0x3) << 4);
			break;

		case clk_mux_flexspi:
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(0x3 << 29)) | ((val & 0x3) << 29);
			break;

		case clk_mux_usdhc2:
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(1 << 17)) | ((val & 1) << 17);
			break;

		case clk_mux_usdhc1:
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(1 << 16)) | ((val & 1) << 16);
			break;

		case clk_mux_sai3:
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(0x3 << 14)) | ((val & 0x3) << 14);
			break;

		case clk_mux_sai2:
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(0x3 << 12)) | ((val & 0x3) << 12);
			break;

		case clk_mux_sai1:
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(0x3 << 10)) | ((val & 0x3) << 10);
			break;

		case clk_mux_perclk:
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(1 << 6)) | ((val & 1) << 6);
			break;

		case clk_mux_flexio2:
			*(imxrt_common.ccm + ccm_cscmr2) = (*(imxrt_common.ccm + ccm_cscmr2) & ~(0x3 << 19)) | ((val & 0x3) << 19);
			break;

		case clk_mux_can:
			*(imxrt_common.ccm + ccm_cscmr2) = (*(imxrt_common.ccm + ccm_cscmr2) & ~(0x3 << 8)) | ((val & 0x3) << 8);
			break;

		case clk_mux_uart:
			*(imxrt_common.ccm + ccm_cscdr1) = (*(imxrt_common.ccm + ccm_cscdr1) & ~(1 << 6)) | ((val & 1) << 6);
			break;

		case clk_mux_enc:
			*(imxrt_common.ccm + ccm_cs2cdr) = (*(imxrt_common.ccm + ccm_cs2cdr) & ~(0x7 << 15)) | ((val & 0x7) << 15);
			break;

		case clk_mux_ldbDi1:
			*(imxrt_common.ccm + ccm_cs2cdr) = (*(imxrt_common.ccm + ccm_cs2cdr) & ~(0x7 << 12)) | ((val & 0x7) << 12);
			break;

		case clk_mux_ldbDi0:
			*(imxrt_common.ccm + ccm_cs2cdr) = (*(imxrt_common.ccm + ccm_cs2cdr) & ~(0x7 << 9)) | ((val & 0x7) << 9);
			break;

		case clk_mux_spdif:
			*(imxrt_common.ccm + ccm_cdcdr) = (*(imxrt_common.ccm + ccm_cdcdr) & ~(0x3 << 20)) | ((val & 0x3) << 20);
			break;

		case clk_mux_flexio1:
			*(imxrt_common.ccm + ccm_cdcdr) = (*(imxrt_common.ccm + ccm_cdcdr) & ~(0x3 << 7)) | ((val & 0x3) << 7);
			break;

		case clk_mux_lpi2c:
			*(imxrt_common.ccm + ccm_cscdr2) = (*(imxrt_common.ccm + ccm_cscdr2) & ~(1 << 18)) | ((val & 1) << 18);
			break;

		case clk_mux_lcdif1pre:
			*(imxrt_common.ccm + ccm_cscdr2) = (*(imxrt_common.ccm + ccm_cscdr2) & ~(0x7 << 15)) | ((val & 0x7) << 15);
			break;

		case clk_mux_lcdif1:
			*(imxrt_common.ccm + ccm_cscdr2) = (*(imxrt_common.ccm + ccm_cscdr2) & ~(0x7 << 9)) | ((val & 0x7) << 9);
			break;

		case clk_mux_csi:
			*(imxrt_common.ccm + ccm_cscdr3) = (*(imxrt_common.ccm + ccm_cscdr3) & ~(0x3 << 9)) | ((val & 0x3) << 9);
			break;
	}
}


u32 _imxrt_ccmGetMux(int mux)
{
	u32 val = 0;

	switch (mux) {
		case clk_mux_pll3:
			val = *(imxrt_common.ccm + ccm_ccsr) & 1;
			break;

		case clk_mux_periph:
			val = (*(imxrt_common.ccm + ccm_cbcdr) >> 25) & 1;
			break;

		case clk_mux_semcAlt:
			val = (*(imxrt_common.ccm + ccm_cbcdr) >> 7) & 1;
			break;

		case clk_mux_semc:
			val = (*(imxrt_common.ccm + ccm_cbcdr) >> 6) & 1;
			break;

		case clk_mux_prePeriph:
			val = (*(imxrt_common.ccm + ccm_cbcmr) >> 18) & 0x3;
			break;

		case clk_mux_trace:
			val = (*(imxrt_common.ccm + ccm_cbcmr) >> 14) & 0x3;
			break;

		case clk_mux_periphclk2:
			val = (*(imxrt_common.ccm + ccm_cbcmr) >> 12) & 0x3;
			break;

		case clk_mux_lpspi:
			val = (*(imxrt_common.ccm + ccm_cbcmr) >> 4) & 0x3;
			break;

		case clk_mux_flexspi:
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 29) & 0x3;
			break;

		case clk_mux_usdhc2:
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 17) & 1;
			break;

		case clk_mux_usdhc1:
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 16) & 1;
			break;

		case clk_mux_sai3:
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 14) & 0x3;
			break;

		case clk_mux_sai2:
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 12) & 0x3;
			break;

		case clk_mux_sai1:
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 10) & 0x3;
			break;

		case clk_mux_perclk:
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 6) & 1;
			break;

		case clk_mux_flexio2:
			val = (*(imxrt_common.ccm + ccm_cscmr2) >> 19) & 0x3;
			break;

		case clk_mux_can:
			val = (*(imxrt_common.ccm + ccm_cscmr2) >> 8) & 0x3;
			break;

		case clk_mux_uart:
			val = (*(imxrt_common.ccm + ccm_cscdr1) >> 6) & 1;
			break;

		case clk_mux_enc:
			val = (*(imxrt_common.ccm + ccm_cs2cdr) >> 15) & 0x7;
			break;

		case clk_mux_ldbDi1:
			val = (*(imxrt_common.ccm + ccm_cs2cdr) >> 12) & 0x7;
			break;

		case clk_mux_ldbDi0:
			val = (*(imxrt_common.ccm + ccm_cs2cdr) >> 9) & 0x7;
			break;

		case clk_mux_spdif:
			val = (*(imxrt_common.ccm + ccm_cdcdr) >> 20) & 0x3;
			break;

		case clk_mux_flexio1:
			val = (*(imxrt_common.ccm + ccm_cdcdr) >> 7) & 0x3;
			break;

		case clk_mux_lpi2c:
			val = (*(imxrt_common.ccm + ccm_cscdr2) >> 18) & 1;
			break;

		case clk_mux_lcdif1pre:
			val = (*(imxrt_common.ccm + ccm_cscdr2) >> 15) & 0x7;
			break;

		case clk_mux_lcdif1:
			val = (*(imxrt_common.ccm + ccm_cscdr2) >> 9) & 0x7;
			break;

		case clk_mux_csi:
			val = (*(imxrt_common.ccm + ccm_cscdr3) >> 9) & 0x3;
			break;
	}

	return val;
}


void _imxrt_ccmSetDiv(int div, u32 val)
{
	switch (div) {
		case clk_div_arm: /* CACRR */
			*(imxrt_common.ccm + ccm_cacrr) = (*(imxrt_common.ccm + ccm_cacrr) & ~0x7) | (val & 0x7);
			while (*(imxrt_common.ccm + ccm_cdhipr) & (1 << 16));
			break;

		case clk_div_periphclk2: /* CBCDR */
			*(imxrt_common.ccm + ccm_cbcdr) = (*(imxrt_common.ccm + ccm_cbcdr) & ~(0x7 << 27)) | ((val & 0x7) << 27);
			break;

		case clk_div_semc: /* CBCDR */
			*(imxrt_common.ccm + ccm_cbcdr) = (*(imxrt_common.ccm + ccm_cbcdr) & ~(0x7 << 16)) | ((val & 0x7) << 16);
			while (*(imxrt_common.ccm + ccm_cdhipr) & 1);
			break;

		case clk_div_ahb: /* CBCDR */
			*(imxrt_common.ccm + ccm_cbcdr) = (*(imxrt_common.ccm + ccm_cbcdr) & ~(0x7 << 10)) | ((val & 0x7) << 10);
			while (*(imxrt_common.ccm + ccm_cdhipr) & (1 << 1));
			break;

		case clk_div_ipg: /* CBCDR */
			*(imxrt_common.ccm + ccm_cbcdr) = (*(imxrt_common.ccm + ccm_cbcdr) & ~(0x3 << 8)) | ((val & 0x3) << 8);
			break;

		case clk_div_lpspi: /* CBCMR */
			*(imxrt_common.ccm + ccm_cbcmr) = (*(imxrt_common.ccm + ccm_cbcmr) & ~(0x7 << 26)) | ((val & 0x7) << 26);
			break;

		case clk_div_lcdif1: /* CBCMR */
			*(imxrt_common.ccm + ccm_cbcmr) = (*(imxrt_common.ccm + ccm_cbcmr) & ~(0x7 << 23)) | ((val & 0x7) << 23);
			break;

		case clk_div_flexspi: /* CSCMR1 */
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~(0x7 << 23)) | ((val & 0x7) << 23);
			break;

		case clk_div_perclk: /* CSCMR1 */
			*(imxrt_common.ccm + ccm_cscmr1) = (*(imxrt_common.ccm + ccm_cscmr1) & ~0x3f) | (val & 0x3f);
			break;

		case clk_div_ldbDi1: /* CSCMR2 */
			*(imxrt_common.ccm + ccm_cscmr2) = (*(imxrt_common.ccm + ccm_cscmr2) & ~(1 << 11)) | ((val & 1) << 11);
			break;

		case clk_div_ldbDi0: /* CSCMR2 */
			*(imxrt_common.ccm + ccm_cscmr2) = (*(imxrt_common.ccm + ccm_cscmr2) & ~(1 << 10)) | ((val & 1) << 10);
			break;

		case clk_div_can: /* CSCMR2 */
			*(imxrt_common.ccm + ccm_cscmr2) = (*(imxrt_common.ccm + ccm_cscmr2) & ~(0x3f << 2)) | ((val & 0x3f) << 2);
			break;

		case clk_div_trace: /* CSCDR1 */
			*(imxrt_common.ccm + ccm_cscdr1) = (*(imxrt_common.ccm + ccm_cscdr1) & ~(0x7 << 25)) | ((val & 0x7) << 25);
			break;

		case clk_div_usdhc2: /* CSCDR1 */
			*(imxrt_common.ccm + ccm_cscdr1) = (*(imxrt_common.ccm + ccm_cscdr1) & ~(0x7 << 16)) | ((val & 0x7) << 16);
			break;

		case clk_div_usdhc1: /* CSCDR1 */
			*(imxrt_common.ccm + ccm_cscdr1) = (*(imxrt_common.ccm + ccm_cscdr1) & ~(0x7 << 11)) | ((val & 0x7) << 11);
			break;

		case clk_div_uart: /* CSCDR1 */
			*(imxrt_common.ccm + ccm_cscdr1) = (*(imxrt_common.ccm + ccm_cscdr1) & ~0x3f) | (val & 0x3f);
			break;

		case clk_div_flexio2: /* CS1CDR */
			*(imxrt_common.ccm + ccm_cs1cdr) = (*(imxrt_common.ccm + ccm_cs1cdr) & ~(0x7 << 25)) | ((val & 0x7) << 25);
			break;

		case clk_div_sai3pre: /* CS1CDR */
			*(imxrt_common.ccm + ccm_cs1cdr) = (*(imxrt_common.ccm + ccm_cs1cdr) & ~(0x7 << 22)) | ((val & 0x7) << 22);
			break;

		case clk_div_sai3: /* CS1CDR */
			*(imxrt_common.ccm + ccm_cs1cdr) = (*(imxrt_common.ccm + ccm_cs1cdr) & ~(0x3f << 16)) | ((val & 0x3f) << 16);
			break;

		case clk_div_flexio2pre: /* CS1CDR */
			*(imxrt_common.ccm + ccm_cs1cdr) = (*(imxrt_common.ccm + ccm_cs1cdr) & ~(0x7 << 9)) | ((val & 0x7) << 9);
			break;

		case clk_div_sai1pre: /* CS1CDR */
			*(imxrt_common.ccm + ccm_cs1cdr) = (*(imxrt_common.ccm + ccm_cs1cdr) & ~(0x7 << 6)) | ((val & 0x7) << 6);
			break;

		case clk_div_sai1: /* CS1CDR */
			*(imxrt_common.ccm + ccm_cs1cdr) = (*(imxrt_common.ccm + ccm_cs1cdr) & ~0x3f) | (val & 0x3f);
			break;

		case clk_div_enc: /* CS2CDR */
			*(imxrt_common.ccm + ccm_cs2cdr) = (*(imxrt_common.ccm + ccm_cs2cdr) & ~(0x3f << 21)) | ((val & 0x3f) << 21);
			break;

		case clk_div_encpre: /* CS2CDR */
			*(imxrt_common.ccm + ccm_cs2cdr) = (*(imxrt_common.ccm + ccm_cs2cdr) & ~(0x7 << 18)) | ((val & 0x7) << 18);
			break;

		case clk_div_sai2pre: /* CS2CDR */
			*(imxrt_common.ccm + ccm_cs2cdr) = (*(imxrt_common.ccm + ccm_cs2cdr) & ~(0x7 << 6)) | ((val & 0x7) << 6);
			break;

		case clk_div_sai2: /* CS2CDR */
			*(imxrt_common.ccm + ccm_cs2cdr) = (*(imxrt_common.ccm + ccm_cs2cdr) & ~0x3f) | (val & 0x3f);
			break;

		case clk_div_spdif0pre: /* CDCDR */
			*(imxrt_common.ccm + ccm_cdcdr) = (*(imxrt_common.ccm + ccm_cdcdr) & ~(0x7 << 25)) | ((val & 0x7) << 25);
			break;

		case clk_div_spdif0: /* CDCDR */
			*(imxrt_common.ccm + ccm_cdcdr) = (*(imxrt_common.ccm + ccm_cdcdr) & ~(0x7 << 22)) | ((val & 0x7) << 22);
			break;

		case clk_div_flexio1pre: /* CDCDR */
			*(imxrt_common.ccm + ccm_cdcdr) = (*(imxrt_common.ccm + ccm_cdcdr) & ~(0x7 << 12)) | ((val & 0x7) << 12);
			break;

		case clk_div_flexio1: /* CDCDR */
			*(imxrt_common.ccm + ccm_cdcdr) = (*(imxrt_common.ccm + ccm_cdcdr) & ~(0x7 << 9)) | ((val & 0x7) << 9);
			break;

		case clk_div_lpi2c: /* CSCDR2 */
			*(imxrt_common.ccm + ccm_cscdr2) = (*(imxrt_common.ccm + ccm_cscdr2) & ~(0x3f << 19)) | ((val & 0x3f) << 19);
			break;

		case clk_div_lcdif1pre: /* CSCDR2 */
			*(imxrt_common.ccm + ccm_cscdr2) = (*(imxrt_common.ccm + ccm_cscdr2) & ~(0x7 << 12)) | ((val & 0x7) << 12);
			break;

		case clk_div_csi: /* CSCDR3 */
			*(imxrt_common.ccm + ccm_cscdr3) = (*(imxrt_common.ccm + ccm_cscdr3) & ~(0x7 << 11)) | ((val & 0x7) << 11);
			break;
	}
}


u32 _imxrt_ccmGetDiv(int div)
{
	u32 val = 0;

	switch (div) {
		case clk_div_arm: /* CACRR */
			val = *(imxrt_common.ccm + ccm_cacrr) & 0x7;
			break;

		case clk_div_periphclk2: /* CBCDR */
			val = (*(imxrt_common.ccm + ccm_cbcdr) >> 27) & 0x7;
			break;

		case clk_div_semc: /* CBCDR */
			val = (*(imxrt_common.ccm + ccm_cbcdr) >> 16) & 0x7;
			break;

		case clk_div_ahb: /* CBCDR */
			val = (*(imxrt_common.ccm + ccm_cbcdr) >> 10) & 0x7;
			break;

		case clk_div_ipg: /* CBCDR */
			val = (*(imxrt_common.ccm + ccm_cbcdr) >> 8) & 0x3;
			break;

		case clk_div_lpspi: /* CBCMR */
			val = (*(imxrt_common.ccm + ccm_cbcmr) >> 26) & 0x7;
			break;

		case clk_div_lcdif1: /* CBCMR */
			val = (*(imxrt_common.ccm + ccm_cbcmr) >> 23) & 0x7;
			break;

		case clk_div_flexspi: /* CSCMR1 */
			val = (*(imxrt_common.ccm + ccm_cscmr1) >> 23) & 0x7;
			break;

		case clk_div_perclk: /* CSCMR1 */
			val = *(imxrt_common.ccm + ccm_cscmr1) & 0x3f;
			break;

		case clk_div_ldbDi1: /* CSCMR2 */
			val = (*(imxrt_common.ccm + ccm_cscmr2) >> 11) & 1;
			break;

		case clk_div_ldbDi0: /* CSCMR2 */
			val = (*(imxrt_common.ccm + ccm_cscmr2) >> 10) & 1;
			break;

		case clk_div_can: /* CSCMR2 */
			val = (*(imxrt_common.ccm + ccm_cscmr2) >> 2) & 0x3f;
			break;

		case clk_div_trace: /* CSCDR1 */
			val = (*(imxrt_common.ccm + ccm_cscdr1) >> 25) & 0x7;
			break;

		case clk_div_usdhc2: /* CSCDR1 */
			val = (*(imxrt_common.ccm + ccm_cscdr1) >> 16) & 0x7;
			break;

		case clk_div_usdhc1: /* CSCDR1 */
			val = (*(imxrt_common.ccm + ccm_cscdr1) >> 11) & 0x7;
			break;

		case clk_div_uart: /* CSCDR1 */
			val = *(imxrt_common.ccm + ccm_cscdr1) & 0x3f;
			break;

		case clk_div_flexio2: /* CS1CDR */
			val = (*(imxrt_common.ccm + ccm_cs1cdr) >> 25) & 0x7;
			break;

		case clk_div_sai3pre: /* CS1CDR */
			val = (*(imxrt_common.ccm + ccm_cs1cdr) >> 22) & 0x7;
			break;

		case clk_div_sai3: /* CS1CDR */
			val = (*(imxrt_common.ccm + ccm_cs1cdr) >> 16) & 0x3f;
			break;

		case clk_div_flexio2pre: /* CS1CDR */
			val = (*(imxrt_common.ccm + ccm_cs1cdr) >> 9) & 0x7;
			break;

		case clk_div_sai1pre: /* CS1CDR */
			val = (*(imxrt_common.ccm + ccm_cs1cdr) >> 6) & 0x7;
			break;

		case clk_div_sai1: /* CS1CDR */
			val = *(imxrt_common.ccm + ccm_cs1cdr) & 0x3f;
			break;

		case clk_div_enc: /* CS2CDR */
			val = (*(imxrt_common.ccm + ccm_cs2cdr) >> 21) & 0x3f;
			break;

		case clk_div_encpre: /* CS2CDR */
			val = (*(imxrt_common.ccm + ccm_cs2cdr) >> 18) & 0x7;
			break;

		case clk_div_sai2pre: /* CS2CDR */
			val = (*(imxrt_common.ccm + ccm_cs2cdr) >> 6) & 0x7;
			break;

		case clk_div_sai2: /* CS2CDR */
			val = *(imxrt_common.ccm + ccm_cs2cdr) & 0x3f;
			break;

		case clk_div_spdif0pre: /* CDCDR */
			val = (*(imxrt_common.ccm + ccm_cdcdr) >> 25) & 0x7;
			break;

		case clk_div_spdif0: /* CDCDR */
			val = (*(imxrt_common.ccm + ccm_cdcdr) >> 22) & 0x7;
			break;

		case clk_div_flexio1pre: /* CDCDR */
			val = (*(imxrt_common.ccm + ccm_cdcdr) >> 12) & 0x7;
			break;

		case clk_div_flexio1: /* CDCDR */
			val = (*(imxrt_common.ccm + ccm_cdcdr) >> 9) & 0x7;
			break;

		case clk_div_lpi2c: /* CSCDR2 */
			val = (*(imxrt_common.ccm + ccm_cscdr2) >> 19) & 0x3f;
			break;

		case clk_div_lcdif1pre: /* CSCDR2 */
			val = (*(imxrt_common.ccm + ccm_cscdr2) >> 12) & 0x7;
			break;

		case clk_div_csi: /* CSCDR3 */
			val = (*(imxrt_common.ccm + ccm_cscdr3) >> 11) & 0x7;
			break;
	}

	return val;
}


void _imxrt_ccmControlGate(int dev, int state)
{
	int index = dev >> 4, shift = (dev & 0xf) << 1;
	u32 t;

	if (index > 7)
		return;

	t = *(imxrt_common.ccm + ccm_ccgr0 + index) & ~(0x3 << shift);
	*(imxrt_common.ccm + ccm_ccgr0 + index) = t | ((state & 0x3) << shift);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _imxrt_ccmSetMode(int mode)
{
	*(imxrt_common.ccm + ccm_clpcr) = (*(imxrt_common.ccm + ccm_clpcr) & ~0x3) | (mode & 0x3);
}


/* SCB */


void _imxrt_scbSetPriorityGrouping(u32 group)
{
	u32 t;

	/* Get register value and clear bits to set */
	t = *(imxrt_common.scb + scb_aircr) & ~0xffff0700;

	/* Store new value */
	*(imxrt_common.scb + scb_aircr) = t | 0x5fa0000 | ((group & 7) << 8);
}


u32 _imxrt_scbGetPriorityGrouping(void)
{
	return (*(imxrt_common.scb + scb_aircr) & 0x700) >> 8;
}


void _imxrt_scbSetPriority(s8 excpn, u32 priority)
{
	volatile u8 *ptr;

	ptr = &((u8*)(imxrt_common.scb + scb_shp0))[excpn - 4];

	*ptr = (priority << 4) & 0x0ff;
}


u32 _imxrt_scbGetPriority(s8 excpn)
{
	volatile u8 *ptr;

	ptr = &((u8*)(imxrt_common.scb + scb_shp0))[excpn - 4];

	return *ptr >> 4;
}


/* NVIC (Nested Vectored Interrupt Controller */


void _imxrt_nvicSetIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = imxrt_common.nvic + ((u8)irqn >> 5) + (state ? nvic_iser: nvic_icer);
	*ptr |= 1 << (irqn & 0x1F);
}


u32 _imxrt_nvicGetPendingIRQ(s8 irqn)
{
	volatile u32 *ptr = imxrt_common.nvic + ((u8)irqn >> 5) + nvic_ispr;
	return !!(*ptr & (1 << (irqn & 0x1F)));
}


void _imxrt_nvicSetPendingIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = imxrt_common.nvic + ((u8)irqn >> 5) + (state ? nvic_ispr: nvic_icpr);
	*ptr |= 1 << (irqn & 0x1F);
}


u32 _imxrt_nvicGetActive(s8 irqn)
{
	volatile u32 *ptr = imxrt_common.nvic + ((u8)irqn >> 5) + nvic_iabr;
	return !!(*ptr & (1 << (irqn & 0x1F)));
}


void _imxrt_nvicSetPriority(s8 irqn, u32 priority)
{
	volatile u8 *ptr;

	ptr = (u8*)(imxrt_common.nvic + irqn + nvic_ip);

	*ptr = (priority << 4) & 0x0ff;
}


u8 _imxrt_nvicGetPriority(s8 irqn)
{
	volatile u8 *ptr;

	ptr = (u8*)(imxrt_common.nvic + irqn + nvic_ip);

	return *ptr >> 4;
}


void _imxrt_nvicSystemReset(void)
{
	*(imxrt_common.scb + scb_aircr) = ((0x5fa << 16) | (*(imxrt_common.scb + scb_aircr) & (0x700)) | (1 << 0x02));

	__asm__ volatile ("dsb");

	for(;;);
}


/* SysTick */


int _imxrt_systickInit(u32 interval)
{
	u64 load = ((u64) interval * imxrt_common.cpuclk) / 1000000;
	if (load > 0x00ffffff)
		return -EINVAL;

	*(imxrt_common.stk + stk_load) = (u32)load;
	*(imxrt_common.stk + stk_ctrl) = 0x7;

	return EOK;
}


void _imxrt_systickSet(u8 state)
{
	*(imxrt_common.stk + stk_ctrl) &= ~(!state);
	*(imxrt_common.stk + stk_ctrl) |= !!state;
}


u32 _imxrt_systickGet(void)
{
	u32 cb;

	cb = ((*(imxrt_common.stk + stk_load) - *(imxrt_common.stk + stk_val)) * 1000) / *(imxrt_common.stk + stk_load);

	/* Add 1000 us if there's systick pending */
	if (*(imxrt_common.scb + scb_icsr) & (1 << 26))
		cb += 1000;

	return cb;
}


/* GPIO */


static volatile u32 *_imxrt_gpioGetReg(unsigned int d)
{
	switch (d) {
		case gpio1:
			return imxrt_common.gpio[0];
		case gpio2:
			return imxrt_common.gpio[1];
		case gpio3:
			return imxrt_common.gpio[2];
		case gpio4:
			return imxrt_common.gpio[3];
		case gpio5:
			return imxrt_common.gpio[4];
	}

	return NULL;
}


int _imxrt_gpioConfig(unsigned int d, u8 pin, u8 dir)
{
	volatile u32 *reg = _imxrt_gpioGetReg(d);

	_imxrt_ccmControlGate(d, 1);

	if (!reg || pin > 31)
		return -EINVAL;

	*(reg + gpio_gdir) &= ~(!dir << pin);
	*(reg + gpio_gdir) |= !!dir << pin;

	return EOK;
}


int _imxrt_gpioSet(unsigned int d, u8 pin, u8 val)
{
	volatile u32 *reg = _imxrt_gpioGetReg(d);

	if (!reg || pin > 31)
		return -EINVAL;

	*(reg + gpio_dr) &= ~(!val << pin);
	*(reg + gpio_dr) |= !!val << pin;

	return EOK;
}


int _imxrt_gpioSetPort(unsigned int d, u32 val)
{
	volatile u32 *reg = _imxrt_gpioGetReg(d);

	if (!reg)
		return -EINVAL;

	*(reg + gpio_dr) = val;

	return EOK;
}


int _imxrt_gpioGet(unsigned int d, u8 pin, u8 *val)
{
	volatile u32 *reg = _imxrt_gpioGetReg(d);

	if (!reg || pin > 31)
		return -EINVAL;

	*val = !!(*(reg + gpio_psr) & (1 << pin));

	return EOK;
}


int _imxrt_gpioGetPort(unsigned int d, u32 *val)
{
	volatile u32 *reg = _imxrt_gpioGetReg(d);

	if (!reg)
		return -EINVAL;

	*val = *(reg + gpio_psr);

	return EOK;
}


/* MPU */


void _imxrt_enableMPU(void)
{
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	*(imxrt_common.scb + scb_shcsr) |= ( 1 << 16);
	*(imxrt_common.mpu + mpu_ctrl) = 0x4 | 1;
}


void _imxrt_disableMPU(void)
{
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	*(imxrt_common.scb + scb_shcsr) &= ~( 1 << 16);
	*(imxrt_common.mpu + mpu_ctrl) = 0;
}


/* Cache */


void _imxrt_enableDCache(void)
{
	u32 ccsidr, sets, ways;

	*(imxrt_common.scb + scb_csselr) = 0;
	hal_cpuDataSyncBarrier();

	ccsidr = *(imxrt_common.scb + scb_ccsidr);

	/* Invalidate D$ */
	for (sets = (ccsidr >> 13) & 0x7fff; sets-- != 0; )
		for (ways = (ccsidr >> 3) & 0x3ff; ways-- != 0; )
			*(imxrt_common.scb + scb_dcisw) = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
	hal_cpuDataSyncBarrier();

	*(imxrt_common.scb + scb_ccr) |= 1 << 16;

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _imxrt_disableDCache(void)
{
	register u32 ccsidr, sets, ways;

	*(imxrt_common.scb + scb_csselr) = 0;
	hal_cpuDataSyncBarrier();

	*(imxrt_common.scb + scb_ccr) &= ~(1 << 16);
	hal_cpuDataSyncBarrier();

	ccsidr = *(imxrt_common.scb + scb_ccsidr);

	for (sets = (ccsidr >> 13) & 0x7fff; sets-- != 0; )
		for (ways = (ccsidr >> 3) & 0x3ff; ways-- != 0; )
			*(imxrt_common.scb + scb_dccisw) = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _imxrt_enableICache(void)
{
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	*(imxrt_common.scb + scb_iciallu) = 0; /* Invalidate I$ */
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	*(imxrt_common.scb + scb_ccr) |= 1 << 17;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _imxrt_disableICache(void)
{
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	*(imxrt_common.scb + scb_ccr) &= ~(1 << 17);
	*(imxrt_common.scb + scb_iciallu) = 0;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


unsigned int _imxrt_cpuid(void)
{
	return *(imxrt_common.scb + scb_cpuid);
}


void _imxrt_wdgReload(void)
{
}


void _imxrt_platformInit(void)
{
	hal_spinlockCreate(&imxrt_common.pltctlSp, "pltctlSp");
}


void _imxrt_init(void)
{
	int i;

	imxrt_common.gpio[0] = (void *)0x401b8000;
	imxrt_common.gpio[1] = (void *)0x401bc000;
	imxrt_common.gpio[2] = (void *)0x401c0000;
	imxrt_common.gpio[3] = (void *)0x401c4000;
	imxrt_common.gpio[4] = (void *)0x400c0000;
	imxrt_common.aips[0] = (void *)0x4007c000;
	imxrt_common.aips[1] = (void *)0x4017c000;
	imxrt_common.aips[2] = (void *)0x4027c000;
	imxrt_common.aips[3] = (void *)0x4037c000;
	imxrt_common.ccm = (void *)0x400fc000;
	imxrt_common.ccm_analog = (void *)0x400d8000;
	imxrt_common.pmu = (void *)0x400d8110;
	imxrt_common.xtalosc = (void *)0x400d8000;
	imxrt_common.iomuxgpr = (void *)0x400ac000;
	imxrt_common.iomuxc = (void *)0x401f8000;
	imxrt_common.iomuxsnvs = (void *)0x400a8000;
	imxrt_common.nvic = (void *)0xe000e100;
	imxrt_common.scb = (void *)0xe000ed00;
	imxrt_common.mpu = (void* ) 0xe000ed90;
	imxrt_common.stk = (void *)0xe000e010;
	imxrt_common.wdog1 = (void *)0x400b8000;
	imxrt_common.wdog2 = (void *)0x400d0000;
	imxrt_common.rtwdog = (void *)0x400bc000;
	imxrt_common.src = (void *)0x400f8000;

	imxrt_common.xtaloscFreq = 24000000;
	imxrt_common.cpuclk = 528000000; /* Default system clock */


	/* Store reset flags and then clean them */
	imxrt_common.resetFlags = *(imxrt_common.src + src_srsr) & 0x1f;
	*(imxrt_common.src + src_srsr) |= 0x1f;

	/* Disable watchdogs */
	if (*(imxrt_common.wdog1 + wdog_wcr) & (1 << 2))
		*(imxrt_common.wdog1 + wdog_wcr) &= ~(1 << 2);
	if (*(imxrt_common.wdog2 + wdog_wcr) & (1 << 2))
		*(imxrt_common.wdog2 + wdog_wcr) &= ~(1 << 2);

	*(imxrt_common.rtwdog + rtwdog_cnt) = 0xd928c520; /* Update key */
	*(imxrt_common.rtwdog + rtwdog_total) = 0xffff;
	*(imxrt_common.rtwdog + rtwdog_cs) = (*(imxrt_common.rtwdog + rtwdog_cs) & ~(1 << 7)) | (1 << 5);

	/* Disable Systick which might be enabled by bootrom */
	if (*(imxrt_common.stk + stk_ctrl) & 1)
		*(imxrt_common.stk + stk_ctrl) &= ~1;

	/* Configure cache */
	_imxrt_enableDCache();
	_imxrt_enableICache();

	_imxrt_ccmControlGate(pctl_clk_iomuxc, clk_state_run_wait);

	_imxrt_ccmSetMux(clk_mux_periphclk2, 0x1);
	_imxrt_ccmSetMux(clk_mux_periph, 0x1);

	/* Configure ARM PLL to 1056M */
	_imxrt_ccmInitArmPll(88);
	_imxrt_ccmInitSysPll(1);
	_imxrt_ccmInitUsb1Pll(0);

	_imxrt_ccmSetDiv(clk_div_arm, 0x1);
	_imxrt_ccmSetDiv(clk_div_ahb, 0x0);
	_imxrt_ccmSetDiv(clk_div_ipg, 0x3);

	/* Now CPU runs again on ARM PLL at 600M (with divider 2) */
	_imxrt_ccmSetMux(clk_mux_prePeriph, 0x3);
	_imxrt_ccmSetMux(clk_mux_periph, 0x0);

	/* Disable unused clocks */
	*(imxrt_common.ccm + ccm_ccgr0) = 0x00c0ffff;
	*(imxrt_common.ccm + ccm_ccgr1) = 0x30000000;
	*(imxrt_common.ccm + ccm_ccgr2) = 0xfffff03f;
	*(imxrt_common.ccm + ccm_ccgr3) = 0xf00c3fff;
	*(imxrt_common.ccm + ccm_ccgr4) = 0x0000ff3c;
	*(imxrt_common.ccm + ccm_ccgr5) = 0xf00f330f;
	*(imxrt_common.ccm + ccm_ccgr6) = 0x00fc0f00;

	/* Remain in run mode on wfi */
	_imxrt_ccmSetMode(0);

	/* Power down all unused PLL */
	_imxrt_ccmDeinitAudioPll();
	_imxrt_ccmDeinitEnetPll();
	_imxrt_ccmDeinitUsb2Pll();

	/* Allow userspace applications to access hardware registers */
	for (i = 0; i < sizeof(imxrt_common.aips) / sizeof(imxrt_common.aips[0]); ++i) {
		*(imxrt_common.aips[i] + aipstz_opacr) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr1) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr2) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr3) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr4) &= ~0x44444444;
	}

	/* Enable UsageFault, BusFault and MemManage exceptions */
	*(imxrt_common.scb + scb_shcsr) |= (1 << 16) | (1 << 17) | (1 << 18);
}
