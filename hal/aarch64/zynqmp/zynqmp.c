/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * zynq-7000 basic peripherals control functions
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/aarch64/aarch64.h"
#include "hal/spinlock.h"
#include "include/arch/aarch64/zynqmp/zynqmp.h"

#include "hal/aarch64/halsyspage.h"
#include "hal/aarch64/arch/pmap.h"
#include "zynqmp_regs.h"

#define CSU_BASE_ADDRESS      0xffca0000
#define IOU_SLCR_BASE_ADDRESS 0xff180000
#define CRF_APB_BASE_ADDRESS  0xfd1a0000
#define CRL_APB_BASE_ADDRESS  0xff5e0000
#define DDRC_BASE_ADDRESS     0xfd070000
#define DDR_PHY_BASE_ADDRESS  0xfd080000
#define APU_BASE_ADDRESS      0xfd5c0000
#define RPU_BASE_ADDRESS      0xff9a0000
#define LPD_SLCR_BASE_ADDRESS 0xff410000
#define FPD_SLCR_BASE_ADDRESS 0xfd610000


/* PLO entrypoint */
extern void _start(void);

struct {
	spinlock_t pltctlSp;
	volatile u32 *csu;
	volatile u32 *iou_slcr;
	volatile u32 *apu;
	volatile u32 *crf_apb;
	volatile u32 *crl_apb;
	volatile u32 *ddrc;
	volatile u32 *ddr_phy;
	unsigned int nCpus;
} zynq_common;


volatile unsigned int nCpusStarted = 0;


static int _zynqmp_setBasicGenerator(volatile u32 *reg, int dev, char src, char div0, char div1, char active)
{
	u32 val = src;
	if (dev == pctl_devclock_lpd_timestamp) {
		val &= 0x7;
	}
	else {
		val &= 0x3;
	}

	val |= ((div0 & 0x3f) << 8) | ((div1 & 0x3f) << 16) | (active << 24);
	if (dev == pctl_devclock_lpd_cpu_r5) {
		/* According to docs turning this bit off could lead to system hang - ensure it is on */
		val |= (1 << 24);
	}

	*reg = val;
	hal_cpuDataSyncBarrier();
	return 0;
}


int _zynqmp_setDevClock(int dev, char src, char div0, char div1, char active)
{
	if ((dev >= pctl_devclock_lpd_usb3_dual) && (dev <= pctl_devclock_lpd_timestamp)) {
		unsigned regOffset = (dev - pctl_devclock_lpd_usb3_dual) + crl_apb_usb3_dual_ref_ctrl;
		return _zynqmp_setBasicGenerator(zynq_common.crl_apb + regOffset, dev, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		unsigned regOffset = (dev - pctl_devclock_fpd_acpu) + crf_apb_acpu_ctrl;
		return _zynqmp_setBasicGenerator(zynq_common.crf_apb + regOffset, dev, src, div0, div1, active);
	}

	return -1;
}


static int _zynqmp_getBasicGenerator(volatile u32 *reg, char *src, char *div0, char *div1, char *active)
{
	u32 val = *reg;
	*src = val & 0x7;
	*div0 = (val >> 8) & 0x3f;
	*div1 = (val >> 16) & 0x3f;
	*active = val >> 24;
	return 0;
}


int _zynqmp_getDevClock(int dev, char *src, char *div0, char *div1, char *active)
{
	if ((dev >= pctl_devclock_lpd_usb3_dual) && (dev <= pctl_devclock_lpd_timestamp)) {
		unsigned regOffset = (dev - pctl_devclock_lpd_usb3_dual) + crl_apb_usb3_dual_ref_ctrl;
		return _zynqmp_getBasicGenerator(zynq_common.crl_apb + regOffset, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		unsigned regOffset = (dev - pctl_devclock_fpd_acpu) + crf_apb_acpu_ctrl;
		return _zynqmp_getBasicGenerator(zynq_common.crf_apb + regOffset, src, div0, div1, active);
	}

	return -1;
}


static void _zynqmp_setMIOMuxing(unsigned pin, char l0, char l1, char l2, char l3)
{
	u32 val = ((l0 & 0x1) << 1) | ((l1 & 0x1) << 2) | ((l2 & 0x3) << 3) | ((l3 & 0x7) << 5);
	*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) = (*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & ~0xff) | val;
}


static void _zynqmp_setMIOTristate(unsigned pin, char config)
{
	u32 reg = pin / 32 + iou_slcr_mio_mst_tri0;
	u32 bit = pin % 32;
	u32 mask = 1 << bit;

	if ((config & PCTL_MIO_TRI_ENABLE) != 0) {
		*(zynq_common.iou_slcr + reg) |= mask;
	}
	else {
		*(zynq_common.iou_slcr + reg) &= ~mask;
	}
}

static void _zynqmp_setMIOControl(unsigned pin, char config)
{
	u32 reg = (pin / 26) * (iou_slcr_bank1_ctrl0 - iou_slcr_bank0_ctrl0) + iou_slcr_bank0_ctrl0;
	u32 bit = pin % 26;
	u32 mask = 1 << bit;
	int i;

	for (i = 0; i <= 6; i++) {
		if (i == 2) {
			/* ctrl2 registers don't exist, skip */
			continue;
		}

		if ((config & (1 << i)) != 0) {
			*(zynq_common.iou_slcr + reg + i) |= mask;
		}
		else {
			*(zynq_common.iou_slcr + reg + i) &= ~mask;
		}
	}
}



int _zynqmp_setMIO(unsigned pin, char l0, char l1, char l2, char l3, char config)
{
	if (pin > 77) {
		return -1;
	}

	_zynqmp_setMIOMuxing(pin, l0, l1, l2, l3);
	_zynqmp_setMIOTristate(pin, config);
	_zynqmp_setMIOControl(pin, config);

	return 0;
}


static void _zynqmp_getMIOMuxing(unsigned pin, char *l0, char *l1, char *l2, char *l3)
{
	u32 val = *(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & 0xff;
	*l0 = (val >> 1) & 0x1;
	*l1 = (val >> 2) & 0x1;
	*l2 = (val >> 3) & 0x3;
	*l3 = (val >> 5) & 0x7;
}


static void _zynqmp_getMIOTristate(unsigned pin, char *config)
{
	u32 reg = pin / 32 + iou_slcr_mio_mst_tri0;
	u32 bit = pin % 32;
	if (*(zynq_common.iou_slcr + reg) & (1 << bit)) {
		*config |= PCTL_MIO_TRI_ENABLE;
	}
}

static void _zynqmp_getMIOControl(unsigned pin, char *config)
{
	u32 reg = (pin / 26) * (iou_slcr_bank1_ctrl0 - iou_slcr_bank0_ctrl0) + iou_slcr_bank0_ctrl0;
	u32 bit = pin % 26;
	u32 mask = 1 << bit;
	int i;

	for (i = 0; i <= 6; i++) {
		if (i == 2) {
			/* ctrl2 registers don't exist, skip */
			continue;
		}

		if ((*(zynq_common.iou_slcr + reg + i) & mask) != 0) {
			*config |= (1 << i);
		}
	}
}


static int _zynqmp_getMIO(unsigned pin, char *l0, char *l1, char *l2, char *l3, char *config)
{
	if (pin > 77) {
		return -1;
	}

	*config = 0;
	_zynqmp_getMIOMuxing(pin, l0, l1, l2, l3);
	_zynqmp_getMIOTristate(pin, config);
	_zynqmp_getMIOControl(pin, config);

	return 0;
}


static int _zynqmp_parseReset(int dev, volatile u32 **reg, u32 *bit)
{
	switch (dev) {
			/* clang-format off */
		case pctl_devreset_lpd_gem0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou0; *bit = (1 << 0); break;
		case pctl_devreset_lpd_gem1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou0; *bit = (1 << 1); break;
		case pctl_devreset_lpd_gem2: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou0; *bit = (1 << 2); break;
		case pctl_devreset_lpd_gem3: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou0; *bit = (1 << 3); break;
		case pctl_devreset_lpd_qspi: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 0); break;
		case pctl_devreset_lpd_uart0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 1); break;
		case pctl_devreset_lpd_uart1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 2); break;
		case pctl_devreset_lpd_spi0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 3); break;
		case pctl_devreset_lpd_spi1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 4); break;
		case pctl_devreset_lpd_sdio0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 5); break;
		case pctl_devreset_lpd_sdio1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 6); break;
		case pctl_devreset_lpd_can0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 7); break;
		case pctl_devreset_lpd_can1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 8); break;
		case pctl_devreset_lpd_i2c0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 9); break;
		case pctl_devreset_lpd_i2c1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 10); break;
		case pctl_devreset_lpd_ttc0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 11); break;
		case pctl_devreset_lpd_ttc1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 12); break;
		case pctl_devreset_lpd_ttc2: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 13); break;
		case pctl_devreset_lpd_ttc3: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 14); break;
		case pctl_devreset_lpd_swdt: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 15); break;
		case pctl_devreset_lpd_nand: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 16); break;
		case pctl_devreset_lpd_lpd_dma: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 17); break;
		case pctl_devreset_lpd_gpio: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 18); break;
		case pctl_devreset_lpd_iou_cc: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 19); break;
		case pctl_devreset_lpd_timestamp: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_iou2; *bit = (1 << 20); break;
		case pctl_devreset_lpd_rpu_r50: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 0); break;
		case pctl_devreset_lpd_rpu_r51: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 1); break;
		case pctl_devreset_lpd_rpu_amba: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 2); break;
		case pctl_devreset_lpd_ocm: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 3); break;
		case pctl_devreset_lpd_rpu_pge: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 4); break;
		case pctl_devreset_lpd_usb0_corereset: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 6); break;
		case pctl_devreset_lpd_usb1_corereset: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 7); break;
		case pctl_devreset_lpd_usb0_hiberreset: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 8); break;
		case pctl_devreset_lpd_usb1_hiberreset: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 9); break;
		case pctl_devreset_lpd_usb0_apb: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 10); break;
		case pctl_devreset_lpd_usb1_apb: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 11); break;
		case pctl_devreset_lpd_ipi: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 14); break;
		case pctl_devreset_lpd_apm: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 15); break;
		case pctl_devreset_lpd_rtc: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 16); break;
		case pctl_devreset_lpd_sysmon: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 17); break;
		case pctl_devreset_lpd_s_axi_lpd: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 19); break;
		case pctl_devreset_lpd_lpd_swdt: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 20); break;
		case pctl_devreset_lpd_fpd: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_top; *bit = (1 << 23); break;
		case pctl_devreset_lpd_dbg_fpd: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_dbg; *bit = (1 << 0); break;
		case pctl_devreset_lpd_dbg_lpd: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_dbg; *bit = (1 << 1); break;
		case pctl_devreset_lpd_rpu_dbg0: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_dbg; *bit = (1 << 4); break;
		case pctl_devreset_lpd_rpu_dbg1: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_dbg; *bit = (1 << 5); break;
		case pctl_devreset_lpd_dbg_ack: *reg = zynq_common.crl_apb + crl_apb_rst_lpd_dbg; *bit = (1 << 15); break;
		case pctl_devreset_fpd_sata: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 1); break;
		case pctl_devreset_fpd_gt: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 2); break;
		case pctl_devreset_fpd_gpu: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 3); break;
		case pctl_devreset_fpd_gpu_pp0: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 4); break;
		case pctl_devreset_fpd_gpu_pp1: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 5); break;
		case pctl_devreset_fpd_fpd_dma: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 6); break;
		case pctl_devreset_fpd_s_axi_hpc_0_fpd: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 7); break;
		case pctl_devreset_fpd_s_axi_hpc_1_fpd: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 8); break;
		case pctl_devreset_fpd_s_axi_hp_0_fpd: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 9); break;
		case pctl_devreset_fpd_s_axi_hp_1_fpd: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 10); break;
		case pctl_devreset_fpd_s_axi_hpc_2_fpd: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 11); break;
		case pctl_devreset_fpd_s_axi_hpc_3_fpd: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 12); break;
		case pctl_devreset_fpd_swdt: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 15); break;
		case pctl_devreset_fpd_dp: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 16); break;
		case pctl_devreset_fpd_pcie_ctrl: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 17); break;
		case pctl_devreset_fpd_pcie_bridge: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 18); break;
		case pctl_devreset_fpd_pcie_cfg: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_top; *bit = (1 << 19); break;
		case pctl_devreset_fpd_acpu0: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 0); break;
		case pctl_devreset_fpd_acpu1: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 1); break;
		case pctl_devreset_fpd_acpu2: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 2); break;
		case pctl_devreset_fpd_acpu3: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 3); break;
		case pctl_devreset_fpd_apu_l2: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 8); break;
		case pctl_devreset_fpd_acpu0_pwron: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 10); break;
		case pctl_devreset_fpd_acpu1_pwron: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 11); break;
		case pctl_devreset_fpd_acpu2_pwron: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 12); break;
		case pctl_devreset_fpd_acpu3_pwron: *reg = zynq_common.crf_apb + crf_apb_rst_fpd_apu; *bit = (1 << 13); break;
		case pctl_devreset_fpd_ddr_apm: *reg = zynq_common.crf_apb + crf_apb_rst_ddr_ss; *bit = (1 << 2); break;
		case pctl_devreset_fpd_ddr_reserved: *reg = zynq_common.crf_apb + crf_apb_rst_ddr_ss; *bit = (1 << 3); break;
		default: return -1;
			/* clang-format on */
	}

	return 0;
}


int _zynq_setDevRst(int dev, unsigned int state)
{
	volatile u32 *reg;
	u32 bit;

	if (_zynqmp_parseReset(dev, &reg, &bit) < 0) {
		return -1;
	}

	if (state != 0) {
		*reg |= bit;
	}
	else {
		*reg &= ~bit;
	}

	hal_cpuDataSyncBarrier();
	return 0;
}


static int _zynq_getDevRst(int dev, unsigned int *state)
{
	volatile u32 *reg;
	u32 bit;

	if (_zynqmp_parseReset(dev, &reg, &bit) < 0) {
		return -1;
	}

	*state = ((*reg & bit) != 0) ? 1 : 0;
	return 0;
}


static void zynqmp_softRst(void)
{
	/* Equivalent to PS_SRST_B signal */
	*(zynq_common.crl_apb + crl_apb_reset_ctrl) |= (1 << 4);
}


void hal_cpuReboot(void)
{
	zynqmp_softRst();
}


/* TODO */
void hal_wdgReload(void)
{
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	unsigned int t = 0;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&zynq_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_devclock:
			if (data->action == pctl_set)
				ret = _zynqmp_setDevClock(data->devclock.dev, data->devclock.src, data->devclock.div0, data->devclock.div1, data->devclock.active);
			else if (data->action == pctl_set)
				ret = _zynqmp_getDevClock(data->devclock.dev, &data->devclock.src, &data->devclock.div0, &data->devclock.div1, &data->devclock.active);
			break;

		case pctl_mio:
			if (data->action == pctl_set)
				ret = _zynqmp_setMIO(data->mio.pin, data->mio.l0, data->mio.l1, data->mio.l2, data->mio.l3, data->mio.config);
			else if (data->action == pctl_get)
				ret = _zynqmp_getMIO(data->mio.pin, &data->mio.l0, &data->mio.l1, &data->mio.l2, &data->mio.l3, &data->mio.config);
			break;

		case pctl_devreset:
			if (data->action == pctl_set) {
				ret = _zynq_setDevRst(data->devreset.dev, data->devreset.state);
			}
			else if (data->action == pctl_get) {
				ret = _zynq_getDevRst(data->devreset.dev, &t);
				data->devreset.state = t;
			}
			break;

		case pctl_reboot:
			if ((data->action == pctl_set) && (data->reboot.magic == PCTL_REBOOT_MAGIC)) {
				zynqmp_softRst();
			}
			else if (data->action == pctl_get) {
				ret = 0;
				data->reboot.reason = syspage->hs.resetReason;
			}
			break;

		default:
			break;
	}

	hal_spinlockClear(&zynq_common.pltctlSp, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&zynq_common.pltctlSp, "pltctl");
	zynq_common.csu = _pmap_mapDevice(CSU_BASE_ADDRESS, SIZE_PAGE);
	zynq_common.iou_slcr = _pmap_mapDevice(IOU_SLCR_BASE_ADDRESS, SIZE_PAGE);
	zynq_common.crf_apb = _pmap_mapDevice(CRF_APB_BASE_ADDRESS, SIZE_PAGE);
	zynq_common.crl_apb = _pmap_mapDevice(CRL_APB_BASE_ADDRESS, SIZE_PAGE);
	zynq_common.ddrc = _pmap_mapDevice(DDRC_BASE_ADDRESS, SIZE_PAGE);
	zynq_common.ddr_phy = _pmap_mapDevice(DDR_PHY_BASE_ADDRESS, SIZE_PAGE);
	zynq_common.apu = _pmap_mapDevice(APU_BASE_ADDRESS, SIZE_PAGE);
}


unsigned int hal_cpuGetCount(void)
{
	return zynq_common.nCpus;
}


static u32 checkNumCPUs(void)
{
	/* First check if MPIDR indicates uniprocessor system or no MP extensions */
	u64 mpidr = sysreg_read(mpidr_el1);
	if (((mpidr >> 30) & 0x2) != 0x2) {
		return 1;
	}

	/* TODO: find a better way to detect number of CPUs */
	u32 powerStatus = *(zynq_common.crf_apb + crf_apb_rst_fpd_apu) & 0xf;
	u32 cpusAvailable = 0;
	for (int i = 0; i < 4; i++, powerStatus >>= 1) {
		if ((powerStatus & 0x1) == 1) {
			cpusAvailable++;
		}
	}

	return cpusAvailable;
}


void _hal_cpuInit(void)
{
	zynq_common.nCpus = checkNumCPUs();
	hal_cpuAtomicInc(&nCpusStarted);

	hal_cpuSignalEvent();
	while (hal_cpuAtomicGet(&nCpusStarted) != zynq_common.nCpus) {
		hal_cpuWaitForEvent();
	}
}


void hal_cpuSmpSync(void)
{
	/* TODO: not implemented yet */
}
