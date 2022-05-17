/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * IMX6ULL basic peripherals control functions
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../cpu.h"
#include "../armv7a.h"
#include "../../spinlock.h"
#include "../../include/arch/imx6ull.h"

/* CCM registers */
enum { ccm_ccr = 0, ccm_ccdr, ccm_csr, ccm_ccsr, ccm_cacrr, ccm_cbcdr, ccm_cbcmr,
	ccm_cscmr1, ccm_cscmr2, ccm_cscdr1, ccm_cs1cdr, ccm_cs2cdr, ccm_cdcdr, ccm_chsccdr,
	ccm_cscdr2, ccm_cscdr3, ccm_cdhipr = ccm_cscdr3 + 3, ccm_clpcr = ccm_cdhipr + 3,
	ccm_cisr, ccm_cimr, ccm_ccosr, ccm_cgpr, ccm_ccgr0, ccm_ccgr1, ccm_ccgr2, ccm_ccgr3,
	ccm_ccgr4, ccm_ccgr5, ccm_ccgr6, ccm_cmeor = ccm_ccgr6 + 2 };


/* Reserved slots */
const char ccm_reserved[] = { pctl_clk_asrc + 1, pctl_clk_ipsync_ip2apb_tzasc1_ipg + 1, pctl_clk_pxp + 1,
	pctl_clk_mmdc_core_aclk_fast_core_p0 + 1, pctl_clk_iomux_snvs_gpr + 1, pctl_clk_usdhc2 + 1 };


enum { ccm_analog_pll_arm = 0, ccm_analog_pll_arm_set, ccm_analog_pll_arm_clr, ccm_analog_pll_arm_tog, ccm_analog_pll_usb1,
	ccm_analog_pll_usb1_set, ccm_analog_pll_usb1_clr, ccm_analog_pll_usb1_tog, ccm_analog_pll_usb2, ccm_analog_pll_usb2_set,
	ccm_analog_pll_usb2_clr, ccm_analog_pll_usb2_tog, ccm_analog_pll_sys, ccm_analog_pll_sys_set, ccm_analog_pll_sys_clr,
	ccm_analog_pll_sys_tog, ccm_analog_pll_sys_ss, ccm_analog_pll_sys_num = ccm_analog_pll_sys_ss + 4,
	ccm_analog_pll_sys_denom = ccm_analog_pll_sys_num + 4, ccm_analog_pll_audio = ccm_analog_pll_sys_denom + 4,
	ccm_analog_pll_audio_set, ccm_analog_pll_audio_clr, ccm_analog_pll_audio_tog, ccm_analog_pll_audio_num,
	ccm_analog_pll_audio_denom = ccm_analog_pll_audio_num + 4, ccm_analog_pll_video = ccm_analog_pll_audio_denom + 4,
	ccm_analog_pll_video_set, ccm_analog_pll_video_clr, ccm_analog_pll_video_tog, ccm_analog_pll_video_num,
	ccm_analog_pll_video_denom = ccm_analog_pll_video_num + 4, ccm_analog_pll_enet = ccm_analog_pll_video_denom + 4,
	ccm_analog_pll_enet_set, ccm_analog_pll_enet_clr, ccm_analog_pll_enet_tog, ccm_analog_pfd_480, ccm_analog_pfd_480_set,
	ccm_analog_pfd_480_clr, ccm_analog_pfd_480_tog, ccm_analog_pfd_528, ccm_analog_pfd_528_set, ccm_analog_pfd_528_clr,
	ccm_analog_pfd_528_tog,
	ccm_analog_misc0 = 84, ccm_analog_misc0_set, ccm_analog_misc0_clr, ccm_analog_misc0_tog, ccm_analog_misc1,
	ccm_analog_misc1_set, ccm_analog_misc1_clr, ccm_analog_misc1_tog, ccm_analog_misc2, ccm_analog_misc2_set,
	ccm_analog_misc2_clr, ccm_analog_misc2_tog };

/* WDOG registers */
enum { wdog_wcr = 0, wdog_wsr, wdog_wrsr, wdog_wicr, wdog_wmcr };


enum { src_scr = 0, src_sbmr1, src_srsr, src_sisr = src_srsr + 3, src_sbmr2 = src_sisr + 2, src_gpr1, src_gpr2,
	src_gpr3, src_gpr4, src_gpr5, src_gpr6, src_gpr7, src_gpr8, src_gpr9, src_gpr10 };


struct {
	spinlock_t pltctlSp;

	volatile u32 *ccm;
	volatile u32 *ccm_analog;
	volatile u32 *iomux;
	volatile u32 *iomux_gpr;
	volatile u32 *iomux_snvs;
	volatile u16 *wdog;
	volatile u32 *src;
} imx6ull_common;

/* saved in _init_imx6ull.S */
u32 imx6ull_bootReason;

extern unsigned int _end;


static int _imx6ull_isValidDev(int dev)
{
	int i;

	if (dev < pctl_clk_aips_tz1 || dev > pctl_clk_pwm7)
		return 0;

	for (i = 0; i < sizeof(ccm_reserved) / sizeof(ccm_reserved[0]); ++i) {
		if (dev == ccm_reserved[i])
			return 0;
	}

	return 1;
}


static int _imx6ull_getDevClock(int dev, unsigned int *state)
{
	int ccgr, flag;

	if (!_imx6ull_isValidDev(dev))
		return -1;

	ccgr = dev / 16;
	flag = 3 << (2 * (dev % 16));

	*state = (*(imx6ull_common.ccm + ccm_ccgr0 + ccgr) & flag) >> (2 * (dev % 16));

	return 0;
}


static int _imx6ull_setDevClock(int dev, unsigned int state)
{
	int ccgr, flag, mask;
	u32 r;

	if (!_imx6ull_isValidDev(dev))
		return -1;

	ccgr = dev / 16;
	flag = (state & 3) << (2 * (dev % 16));
	mask = 3 << (2 * (dev % 16));

	r = *(imx6ull_common.ccm + ccm_ccgr0 + ccgr);
	*(imx6ull_common.ccm + ccm_ccgr0 + ccgr) = (r & ~mask) | flag;

	return 0;
}


static int _imx6ull_checkIOgprArg(int field, unsigned int *mask)
{
	if (field < pctl_gpr_dmareq0 || field > pctl_gpr_sm2 ||
		(field > pctl_gpr_ref_epit2 && field < pctl_gpr_tzasc1_byp) ||
		(field > pctl_gpr_ocram_tz_addr && field < pctl_gpr_sm1))
		return -1;

	switch (field) {
		case pctl_gpr_addrs0:
		case pctl_gpr_addrs1:
		case pctl_gpr_addrs2:
		case pctl_gpr_addrs3:
			*mask = 0x3;
			break;

		case pctl_gpr_mqs_clk_div:
			*mask = 0xff;
			break;

		case pctl_gpr_ocram_ctl:
		case pctl_gpr_ocram_status:
		case pctl_gpr_ocram_tz_addr:
			*mask = 0xf;
			break;

		default:
			*mask = 0x1;
			break;
	}

	return 0;
}


static int _imx6ull_setIOgpr(int field, unsigned int val)
{
	unsigned int mask, t;
	int err;

	if ((err = _imx6ull_checkIOgprArg(field, &mask)) != 0)
		return err;

	t = *(imx6ull_common.iomux_gpr+ (field >> 5)) & ~(mask << (field & 0x1f));
	*(imx6ull_common.iomux_gpr + (field >> 5)) = t | (val & mask) << (field & 0x1f);

	return 0;
}


static int _imx6ull_setIOmux(int mux, char sion, char mode)
{
	volatile u32 *base = imx6ull_common.iomux;

	if (mux >= pctl_mux_boot_mode0 && mux <= pctl_mux_tamper9) {
#ifdef CPU_IMX6UL
		mux = (mux - pctl_mux_boot_mode0) + 5;
#else
		mux = (mux - pctl_mux_boot_mode0);
		base = imx6ull_common.iomux_snvs;
#endif
	}
	else if (mux < pctl_mux_jtag_mod || mux > pctl_mux_csi_d7)
		return -1;

	*(base + mux) = (!!sion << 4) | (mode & 0xf);

	return 0;
}


static int _imx6ull_setIOpad(int pad, char hys, char pus, char pue, char pke, char ode, char speed, char dse, char sre)
{
	u32 t;
	volatile u32 *base = imx6ull_common.iomux;

	if (pad >= pctl_pad_test_mode && pad <= pctl_pad_tamper9) {
#ifdef CPU_IMX6UL
		pad = (pad - pctl_pad_test_mode) + 163;
#else
		pad = pad - pctl_pad_test_mode + 12;
		base = imx6ull_common.iomux_gpr;
#endif
	}
	else if (pad < pctl_pad_jtag_mod || pad > pctl_pad_csi_d7)
		return -1;

	t = (!!hys << 16) | ((pus & 0x3) << 14) | (!!pue << 13) | (!!pke << 12);
	t |= (!!ode << 11) | ((speed & 0x3) << 6) | ((dse & 0x7) << 3) | !!sre;
	*(base + pad) = t;

	return 0;
}


static int _imx6ull_setIOisel(int isel, char daisy)
{
	if (isel < pctl_isel_anatop || isel > pctl_isel_usdhc2_wp)
		return -1;

	*(imx6ull_common.iomux + isel) = daisy & 0x7;

	return 0;
}


static int _imx6ull_getIOgpr(int field, unsigned int *val)
{
	unsigned int mask;
	int err;

	if ((err = _imx6ull_checkIOgprArg(field, &mask)) != 0)
		return err;

	*val = (*(imx6ull_common.iomux_gpr + (field >> 5)) >> (field & 0x1f)) & mask;

	return 0;
}


static int _imx6ull_getIOmux(int mux, char *sion, char *mode)
{
	u32 t;
	volatile u32 *base = imx6ull_common.iomux;

	if (sion == NULL || mode == NULL)
		return -1;

	if (mux >= pctl_mux_boot_mode0 && mux <= pctl_mux_tamper9) {
#ifdef CPU_IMX6UL
		mux = (mux - pctl_mux_boot_mode0) + 5;
#else
		mux = (mux - pctl_mux_boot_mode0);
		base = imx6ull_common.iomux_snvs;
#endif
	}
	else if (mux < pctl_mux_jtag_mod || mux > pctl_mux_csi_d7)
		return -1;

	t = *(base + mux);

	*sion = !!(t & (1 << 4));
	*mode = t & 0xf;

	return 0;
}


static int _imx6ull_getIOpad(int pad, char *hys, char *pus, char *pue, char *pke, char *ode, char *speed, char *dse, char *sre)
{
	u32 t;
	volatile u32 *base = imx6ull_common.iomux;

	if (hys == NULL || pus == NULL || pue == NULL || pke == NULL || ode == NULL || speed == NULL || dse == NULL || sre == NULL)
		return -1;

	if (pad >= pctl_pad_test_mode && pad <= pctl_pad_tamper9) {
#ifdef CPU_IMX6UL
		pad = (pad - pctl_pad_test_mode) + 163;
#else
		pad = pad - pctl_pad_test_mode + 12;
		base = imx6ull_common.iomux_gpr;
#endif
	}
	else if (pad < pctl_pad_jtag_mod || pad > pctl_pad_csi_d7)
		return -1;

	t = *(base + pad);

	*hys = (t >> 16) & 0x1;
	*pus = (t >> 14) & 0x3;
	*pue = (t >> 13) & 0x1;
	*pke = (t >> 12) & 0x1;
	*ode = (t >> 11) & 0x1;
	*speed = (t >> 6) & 0x3;
	*dse = (t >> 3) & 0x7;
	*sre = t & 0x1;

	return 0;
}


static int _imx6ull_getIOisel(int isel, char *daisy)
{
	if (daisy == NULL || isel < pctl_isel_anatop || isel > pctl_isel_usdhc2_wp)
		return -1;

	*daisy = *(imx6ull_common.iomux + isel) & 0x7;

	return 0;
}


static void _imx6ull_reboot(void)
{
	/* assert SRS signal by writing 0 to bit 4 and 1 to bit 2 (WDOG enable) */
	*(imx6ull_common.wdog + wdog_wcr) = (1 << 2);
	for (;;) ;
}

void hal_wdgReload(void)
{
	*(imx6ull_common.wdog + wdog_wsr) = 0x5555;
	*(imx6ull_common.wdog + wdog_wsr) = 0xAAAA;
}


/* platformctl syscall */

int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	int ret = -1;
	unsigned int t = 0;
	spinlock_ctx_t sc;

	hal_spinlockSet(&imx6ull_common.pltctlSp, &sc);

	switch (data->type) {
	case pctl_devclock:
		if (data->action == pctl_set) {
			ret = _imx6ull_setDevClock(data->devclock.dev, data->devclock.state);
		}
		else if (data->action == pctl_get) {
			ret = _imx6ull_getDevClock(data->devclock.dev, &t);
			data->devclock.state = t;
		}
		break;
	case pctl_iogpr:
		if (data->action == pctl_set) {
			ret = _imx6ull_setIOgpr(data->iogpr.field, data->iogpr.val);
		}
		else if (data->action == pctl_get) {
			ret = _imx6ull_getIOgpr(data->iogpr.field, &t);
			data->iogpr.val = t;
		}
		break;
	case pctl_iomux:
		if (data->action == pctl_set)
			ret = _imx6ull_setIOmux(data->iomux.mux, data->iomux.sion, data->iomux.mode);
		else if (data->action == pctl_get)
			ret = _imx6ull_getIOmux(data->iomux.mux, &data->iomux.sion, &data->iomux.mode);
		break;
	case pctl_iopad:
		if (data->action == pctl_set)
			ret = _imx6ull_setIOpad(data->iopad.pad, data->iopad.hys, data->iopad.pus, data->iopad.pue,
				data->iopad.pke, data->iopad.ode, data->iopad.speed, data->iopad.dse, data->iopad.sre);
		else if (data->action == pctl_get)
			ret = _imx6ull_getIOpad(data->iopad.pad, &data->iopad.hys, &data->iopad.pus, &data->iopad.pue,
				&data->iopad.pke, &data->iopad.ode, &data->iopad.speed, &data->iopad.dse, &data->iopad.sre);
		break;
	case pctl_ioisel:
		if (data->action == pctl_set)
			ret = _imx6ull_setIOisel(data->ioisel.isel, data->ioisel.daisy);
		else if (data->action == pctl_get)
			ret = _imx6ull_getIOisel(data->ioisel.isel, &data->ioisel.daisy);
		break;
	case pctl_reboot:
		if (data->action == pctl_set) {
			if (data->reboot.magic == PCTL_REBOOT_MAGIC) {
				*(imx6ull_common.src + src_gpr10) &= ~(1 << 30);
				hal_cpuInstrBarrier();
				hal_cpuDataMemoryBarrier();
				_imx6ull_reboot();
			}
			else if (data->reboot.magic == PCTL_REBOOT_MAGIC_SECONDARY) {
				*(imx6ull_common.src + src_gpr10) |= 1 << 30;
				hal_cpuInstrBarrier();
				hal_cpuDataMemoryBarrier();
				_imx6ull_reboot();
			}
		}
		else if (data->action == pctl_get) {
			/* [src_gpt10[31:24]] [wdog_wrsr[7:0]] [src_srsr[15:8]] [src_srsr[7:0]] */
			data->reboot.reason = imx6ull_bootReason;
			ret = 0;
		}
		break;
	default:
		break;
	}

	hal_spinlockClear(&imx6ull_common.pltctlSp, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	unsigned int reg, tmp;

	hal_spinlockCreate(&imx6ull_common.pltctlSp, "pltctl");
	imx6ull_common.ccm = (void *)(((u32)&_end + (11 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	imx6ull_common.ccm_analog = (void *)(((u32)&_end + (12 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	imx6ull_common.iomux_snvs = (void *)(((u32)&_end + (13 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	imx6ull_common.iomux = (void *)(((u32)&_end + (14 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	imx6ull_common.iomux_gpr = (void *)(((u32)&_end + (15 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	imx6ull_common.wdog = (void *)(((u32)&_end + (16 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	imx6ull_common.src = (void *)(((u32)&_end + (17 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));

	/* remain in run mode in low power */
	*(imx6ull_common.ccm + ccm_clpcr) &= ~0x3;

	/* kick watchdog power down counter */
	*(imx6ull_common.wdog + wdog_wmcr) = 0;

	/* copy watchdog Reset Status Register to bootreason[23:16] */
	imx6ull_bootReason &= 0xff00ffffu;
	imx6ull_bootReason |= *(imx6ull_common.wdog + wdog_wrsr) << 16;

	/* Set ENFC clock to 198 MHz */
	/* First disable all output clocks */
	reg = *(imx6ull_common.ccm + ccm_ccgr4);
	tmp = reg;
	reg &= ~((3 << 30) | (3 << 28) | (3 << 26) | (3 << 24) | (3 << 12));
	*(imx6ull_common.ccm + ccm_ccgr4) = reg;

	/* Configure ENFC clock */
	reg = *(imx6ull_common.ccm + ccm_cs2cdr);
	reg &= ~((63 << 21) | (7 << 18) | (7 << 15)); /* Clear ENFC clock selector and dividers */
	reg |= (3 << 15);                             /* Set ENFC_CLK_SEL to PLL2 PFD2 (396 MHz) */
	reg |= (1 << 18);                             /* Set ENFC_PRED divider to 2 */
	*(imx6ull_common.ccm + ccm_cs2cdr) = reg;

	/* Restore output clocks state */
	*(imx6ull_common.ccm + ccm_ccgr4) = tmp;
}
