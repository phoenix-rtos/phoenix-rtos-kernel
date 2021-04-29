/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * i.MX RT1170 basic peripherals control functions
 *
 * Copyright 2017, 2019 Phoenix Systems
 * Author: Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "imxrt117x.h"
#include "interrupts.h"
#include "pmap.h"
#include "../../../include/errno.h"
#include "../../../include/arch/imxrt1170.h"


enum { stk_ctrl = 0, stk_load, stk_val, stk_calib };


enum { aipstz_mpr = 0, aipstz_opacr = 16, aipstz_opacr1, aipstz_opacr2, aipstz_opacr3, aipstz_opacr4 };


enum { scb_cpuid = 0, scb_icsr, scb_vtor, scb_aircr, scb_scr, scb_ccr, scb_shp0, scb_shp1,
	scb_shp2, scb_shcsr, scb_cfsr, scb_hfsr, scb_dfsr, scb_mmfar, scb_bfar, scb_afsr, scb_pfr0,
	scb_pfr1, scb_dfr, scb_afr, scb_mmfr0, scb_mmfr1, scb_mmfr2, scb_mmf3, scb_isar0, scb_isar1,
	scb_isar2, scb_isar3, scb_isar4, /* reserved */ scb_clidr = 30, scb_ctr, scb_ccsidr, scb_csselr,
	scb_cpacr, /* 93 reserved */ scb_stir = 128, /* 15 reserved */ scb_mvfr0 = 144, scb_mvfr1,
	scb_mvfr2, /* reserved */ scb_iciallu = 148, /* reserved */ scb_icimvau = 150, scb_scimvac,
	scb_dcisw, scb_dccmvau, scb_dccmvac, scb_dccsw, scb_dccimvac, scb_dccisw, /* 6 reserved */
	scb_itcmcr = 164, scb_dtcmcr, scb_ahbpcr, scb_cacr, scb_ahbscr, /* reserved */ scb_abfsr = 170 };


enum { src_scr = 0, src_sbmr1, src_srsr, src_sbmr2 = 7, src_gpr1, src_gpr2, src_gpr3, src_gpr4,
	src_gpr5, src_gpr6, src_gpr7, src_gpr8, src_gpr9, src_gpr10 };


enum { nvic_iser = 0, nvic_icer = 32, nvic_ispr = 64, nvic_icpr = 96, nvic_iabr = 128,
	nvic_ip = 256, nvic_stir = 896 };


enum { wdog_wcr = 0, wdog_wsr, wdog_wrsr, wdog_wicr, wdog_wmcr };


enum { rtwdog_cs = 0, rtwdog_cnt, rtwdog_total, rtwdog_win };


struct {
	volatile u32 *aips[4];
	volatile u32 *nvic;
	volatile u32 *stk;
	volatile u32 *scb;
	volatile u32 *src;
	volatile u16 *wdog1;
	volatile u16 *wdog2;
	volatile u32 *wdog3;
	volatile u32 *iomux_snvs;
	volatile u32 *iomux_lpsr;
	volatile u32 *iomuxc;
	volatile u32 *gpr;
	volatile u32 *ccm;

	spinlock_t pltctlSp;

	u32 resetFlags;
	u32 cpuclk;
} imxrt_common;


unsigned int _imxrt_cpuid(void)
{
	return *(imxrt_common.scb + scb_cpuid);
}


void _imxrt_wdgReload(void)
{
}


/* IOMUX */


static volatile u32 *_imxrt_IOmuxGetReg(int mux)
{
	if (mux < pctl_mux_gpio_emc_b1_00 || mux > pctl_mux_gpio_lpsr_15)
		return NULL;

	if (mux < pctl_mux_wakeup)
		return imxrt_common.iomuxc + 4 + mux - pctl_mux_gpio_emc_b1_00;

	if (mux < pctl_mux_gpio_lpsr_00)
		return imxrt_common.iomux_snvs + mux - pctl_mux_wakeup;

	return imxrt_common.iomux_lpsr + mux - pctl_mux_gpio_lpsr_00;
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
	if (pad < pctl_pad_gpio_emc_b1_00 || pad > pctl_pad_gpio_lpsr_15)
		return NULL;

	if (pad < pctl_pad_test_mode)
		return imxrt_common.iomuxc + pad + 149 - pctl_pad_gpio_emc_b1_00;

	if (pad < pctl_pad_gpio_lpsr_00)
		return imxrt_common.iomux_snvs + pad + 14 - pctl_pad_test_mode;

	return imxrt_common.iomux_lpsr + pad + 16 - pctl_pad_gpio_lpsr_00;
}


int _imxrt_setIOpad(int pad, char sre, char dse, char pue, char pus, char ode, char apc)
{
	u32 t;
	volatile u32 *reg;
	char pull;

	if ((reg = _imxrt_IOpadGetReg(pad)) == NULL)
		return -EINVAL;

	if (pad >= pctl_pad_gpio_emc_b1_00 && pad <= pctl_pad_gpio_disp_b2_15) {
		/* Fields have slightly diffrent meaning... */
		if (!pue)
			pull = 3;
		else if (pus)
			pull = 1;
		else
			pull = 2;

		t = *reg & ~0x1e;
		t |= (!!dse << 1) | (pull << 2) | (!!ode << 4);
	}
	else {
		t = *reg & ~0x1f;
		t |= (!!sre) | (!!dse << 1) | (!!pue << 2) | (!!pus << 3);

		if (pad >= pctl_pad_test_mode && pad <= pctl_pad_gpio_snvs_09) {
			t &= ~(1 << 6);
			t |= !!ode << 6;
		}
		else {
			t &= ~(1 << 5);
			t |= !!ode << 5;
		}
	}

	/* APC field is not documented. Leave it alone for now. */
	//t &= ~(0xf << 28);
	//t |= (apc & 0xf) << 28;

	(*reg) = t;

	return EOK;
}


static int _imxrt_getIOpad(int pad, char *sre, char *dse, char *pue, char *pus, char *ode, char *apc)
{
	u32 t;
	char pull;
	volatile u32 *reg;

	if ((reg = _imxrt_IOpadGetReg(pad)) == NULL)
		return -EINVAL;

	t = (*reg);

	if (pad >= pctl_pad_gpio_emc_b1_00 && pad <= pctl_pad_gpio_disp_b2_15) {
		pull = (t >> 2) & 3;

		if (pull == 3) {
			*pue = 0;
		}
		else {
			*pue = 1;
			if (pull & 1)
				*pus = 1;
			else
				*pus = 0;
		}

		*ode = (t >> 4) & 1;
		/* sre field does not apply, leave it alone */
	}
	else {
		*sre = t & 1;
		*pue = (t >> 2) & 1;
		*pus = (t >> 3) & 1;

		if (pad >= pctl_pad_test_mode && pad <= pctl_pad_gpio_snvs_09)
			*ode = (t >> 6) & 1;
		else
			*ode = (t >> 5) & 1;
	}

	*dse = (t >> 1) & 1;
	*apc = (t >> 28) & 0xf;

	return EOK;
}


static volatile u32 *_imxrt_IOiselGetReg(int isel, u32 *mask)
{
	if (isel < pctl_isel_flexcan1_rx || isel > pctl_isel_sai4_txsync)
		return NULL;

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

	if (isel >= pctl_isel_can3_canrx)
		return imxrt_common.iomux_lpsr + 32 + isel - pctl_isel_can3_canrx;

	return imxrt_common.iomuxc + 294 + isel - pctl_isel_flexcan1_rx;
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
	state = !state;

	*(imxrt_common.stk + stk_ctrl) &= ~state;
	*(imxrt_common.stk + stk_ctrl) |= !state;
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


/* Cache */


void _imxrt_enableDCache(void)
{
	u32 ccsidr, sets, ways;

	*(imxrt_common.scb + scb_csselr) = 0;
	imxrt_dataSyncBarrier();

	ccsidr = *(imxrt_common.scb + scb_ccsidr);

	/* Invalidate D$ */
	sets = (ccsidr >> 13) & 0x7fff;
	do {
		ways = (ccsidr >> 3) & 0x3ff;
		do {
			*(imxrt_common.scb + scb_dcisw) = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
		} while (ways-- != 0);
	} while(sets-- != 0);
	imxrt_dataSyncBarrier();

	*(imxrt_common.scb + scb_ccr) |= 1 << 16;

	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
}


void _imxrt_disableDCache(void)
{
	register u32 ccsidr, sets, ways;

	*(imxrt_common.scb + scb_csselr) = 0;
	imxrt_dataSyncBarrier();

	*(imxrt_common.scb + scb_ccr) &= ~(1 << 16);
	imxrt_dataSyncBarrier();

	ccsidr = *(imxrt_common.scb + scb_ccsidr);

	sets = (ccsidr >> 13) & 0x7fff;
	do {
		ways = (ccsidr >> 3) & 0x3ff;
		do {
			*(imxrt_common.scb + scb_dcisw) = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
		} while (ways-- != 0);
	} while(sets-- != 0);

	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
}


void _imxrt_cleanDCache(void)
{
	register u32 ccsidr, sets, ways;

	*(imxrt_common.scb + scb_csselr) = 0;

	imxrt_dataSyncBarrier();
	ccsidr = *(imxrt_common.scb + scb_ccsidr);

	/* Clean D$ */
	sets = (ccsidr >> 13) & 0x7fff;
	do {
		ways = (ccsidr >> 3) & 0x3ff;
		do {
			*(imxrt_common.scb + scb_dccsw) = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
		} while (ways-- != 0);
	} while(sets-- != 0);

	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
}



void _imxrt_enableICache(void)
{
	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
	*(imxrt_common.scb + scb_iciallu) = 0; /* Invalidate I$ */
	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
	*(imxrt_common.scb + scb_ccr) |= 1 << 17;
	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
}


void _imxrt_disableICache(void)
{
	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
	*(imxrt_common.scb + scb_ccr) &= ~(1 << 17);
	*(imxrt_common.scb + scb_iciallu) = 0;
	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();
}


/* CCM */

int _imxrt_setDevClock(int clock, int div, int mux, int mfd, int mfn, int state)
{
	unsigned int t;
	volatile u32 *reg = imxrt_common.ccm + (clock * 0x20);

	if (clock < pctl_clk_cm7 || clock > pctl_clk_ccm_clko2)
		return -1;

	t = *reg & ~0x01ff07ffu;
	*reg = t | (!state << 24) | ((mfn & 0xf) << 20) | ((mfd & 0xf) << 16) | ((mux & 0x7) << 8) | (div & 0xff);

	imxrt_dataSyncBarrier();
	imxrt_dataInstrBarrier();

	return 0;
}


int _imxrt_getDevClock(int clock, int *div, int *mux, int *mfd, int *mfn, int *state)
{
	unsigned int t;
	volatile u32 *reg = imxrt_common.ccm + (clock * 0x20);

	if (clock < pctl_clk_cm7 || clock > pctl_clk_ccm_clko2)
		return -1;

	t = *reg;

	*div = t & 0xff;
	*mux = (t >> 8) & 0x7;
	*mfd = (t >> 16) & 0xf;
	*mfn = (t >> 20) & 0xf;
	*state = !(t & (1 << 24));

	return 0;
}


/* GPR */


static int _imxrt_setIOgpr(int which, unsigned int what)
{
	if (which < 0 || which > 76)
		return -EINVAL;

	*(imxrt_common.gpr + which) = what;

	return  0;
}


static int _imxrt_getIOgpr(int which, unsigned int *what)
{
	if (which < 0 || which > 76 || what == NULL)
		return -EINVAL;

	*what = *(imxrt_common.gpr + which);

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
			if (!(ret = _imxrt_getDevClock(data->devclock.dev, &div, &mux, &mfd, &mfn, &state))) {
				data->devclock.div = div;
				data->devclock.mux = mux;
				data->devclock.mfd = mfd;
				data->devclock.mfn = mfn;
				data->devclock.state = state;
			}
		}
		break;

	case pctl_iogpr:
		if (data->action == pctl_set) {
			ret = _imxrt_setIOgpr(data->iogpr.field, data->iogpr.val);
		}
		else if (data->action == pctl_get) {
			if ((ret = _imxrt_getIOgpr(data->iogpr.field, &t)) == 0)
				data->iogpr.val = t;
		}
		break;

	case pctl_iomux:
		if (data->action == pctl_set)
			ret = _imxrt_setIOmux(data->iomux.mux, data->iomux.sion, data->iomux.mode);
		else if (data->action == pctl_get)
			ret = _imxrt_getIOmux(data->iomux.mux, &data->iomux.sion, &data->iomux.mode);
		break;

	case pctl_iopad:
		if (data->action == pctl_set)
			ret = _imxrt_setIOpad(data->iopad.pad, data->iopad.sre, data->iopad.dse, data->iopad.pue,
				data->iopad.pus, data->iopad.ode, data->iopad.apc);
		else if (data->action == pctl_get)
			ret = _imxrt_getIOpad(data->iopad.pad, &data->iopad.sre, &data->iopad.dse, &data->iopad.pue,
				&data->iopad.pus, &data->iopad.ode, &data->iopad.apc);
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
				_imxrt_nvicSystemReset();
		}
		else if (data->action == pctl_get) {
			data->reboot.reason = imxrt_common.resetFlags;
			ret = EOK;
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


void _imxrt_init(void)
{
	imxrt_common.aips[0] = (void *)0x40000000;
	imxrt_common.aips[1] = (void *)0x40400000;
	imxrt_common.aips[2] = (void *)0x40800000;
	imxrt_common.aips[3] = (void *)0x40c00000;
	imxrt_common.ccm = (void *)0x40cc0000;
	imxrt_common.nvic = (void *)0xe000e100;
	imxrt_common.scb = (void *)0xe000ed00;
	imxrt_common.stk = (void *)0xe000e010;
	imxrt_common.wdog1 = (void *)0x40030000;
	imxrt_common.wdog2 = (void *)0x40034000;
	imxrt_common.wdog3 = (void *)0x40038000;
	imxrt_common.src = (void *)0x40c04000;
	imxrt_common.iomux_snvs = (void *)0x40c94000;
	imxrt_common.iomux_lpsr = (void *)0x40c08000;
	imxrt_common.iomuxc = (void *)0x400e8000;
	imxrt_common.gpr = (void *)0x400e4000;

	imxrt_common.cpuclk = 696000000;

	/* Store reset flags and then clean them */
	imxrt_common.resetFlags = *(imxrt_common.src + src_srsr) & 0x1f;
	*(imxrt_common.src + src_srsr) |= 0x1f;

	/* Disable watchdogs */
	if (*(imxrt_common.wdog1 + wdog_wcr) & (1 << 2))
		*(imxrt_common.wdog1 + wdog_wcr) &= ~(1 << 2);
	if (*(imxrt_common.wdog2 + wdog_wcr) & (1 << 2))
		*(imxrt_common.wdog2 + wdog_wcr) &= ~(1 << 2);

	*(imxrt_common.wdog3 + rtwdog_cnt) = 0xd928c520; /* Update key */
	*(imxrt_common.wdog3 + rtwdog_total) = 0xffff;
	*(imxrt_common.wdog3 + rtwdog_cs) |= 1 << 5;
	*(imxrt_common.wdog3 + rtwdog_cs) &= ~(1 << 7);

	/* Disable Systick which might be enabled by bootrom */
	if (*(imxrt_common.stk + stk_ctrl) & 1)
		*(imxrt_common.stk + stk_ctrl) &= ~1;

	/* Configure cache */
	_imxrt_enableDCache();
	_imxrt_enableICache();

	/* Allow userspace applications to access hardware registers */
/*
	for (i = 0; i < sizeof(imxrt_common.aips) / sizeof(imxrt_common.aips[0]); ++i) {
		*(imxrt_common.aips[i] + aipstz_opacr) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr1) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr2) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr3) &= ~0x44444444;
		*(imxrt_common.aips[i] + aipstz_opacr4) &= ~0x44444444;
	}
*/

	/* Enable UsageFault, BusFault and MemManage exceptions */
	*(imxrt_common.scb + scb_shcsr) |= (1 << 16) | (1 << 17) | (1 << 18);
}
