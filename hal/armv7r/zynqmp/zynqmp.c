/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ZynqMP internal peripheral control functions
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/armv7r/armv7r.h"
#include "hal/spinlock.h"
#include "include/arch/armv7r/zynqmp/zynqmp.h"

#include "hal/armv7r/halsyspage.h"
#include "zynqmp_regs.h" /*TBC - hal/armv7r/zynqmp_regs.h*/

#define IOU_SLCR_BASE_ADDRESS 0xff180000U
#define APU_BASE_ADDRESS      0xfd5c0000U
#define CRF_APB_BASE_ADDRESS  0xfd1a0000U
#define CRL_APB_BASE_ADDRESS  0xff5e0000U


/* PLO entrypoint */
extern void _start(void);

struct {
	volatile u32 *iou_slcr;
	volatile u32 *crf_apb;
	volatile u32 *crl_apb;
	spinlock_t pltctlSp;
} zynq_common;


static int _zynqmp_setBasicGenerator(volatile u32 *reg, u32 dev, u8 src, u8 div0, u8 div1, u8 active)
{
	u32 val = src;
	if (dev == (unsigned int)pctl_devclock_lpd_timestamp) {
		val &= 0x7U;
	}
	else {
		val &= 0x3U;
	}

	val |= (((u32)div0 & 0x3fU) << 8) | (((u32)div1 & 0x3fU) << 16) | ((u32)active << 24);
	if (dev == (unsigned int)pctl_devclock_lpd_cpu_r5) {
		/* According to docs turning this bit off could lead to system hang - ensure it is on */
		val |= (0x01UL << 24);
	}

	*reg = val;
	hal_cpuDataSyncBarrier();
	return 0;
}


int _zynqmp_setDevClock(u32 dev, u8 src, u8 div0, u8 div1, u8 active)
{
	if ((dev >= (unsigned int)pctl_devclock_lpd_usb3_dual) && (dev <= (unsigned int)pctl_devclock_lpd_timestamp)) {
		unsigned regOffset = (dev - (unsigned int)pctl_devclock_lpd_usb3_dual) + (unsigned int)crl_apb_usb3_dual_ref_ctrl;
		return _zynqmp_setBasicGenerator(zynq_common.crl_apb + regOffset, dev, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		unsigned regOffset = (dev - (unsigned int)pctl_devclock_fpd_acpu) + crf_apb_acpu_ctrl;
		return _zynqmp_setBasicGenerator(zynq_common.crf_apb + regOffset, dev, src, div0, div1, active);
	}

	return -1;
}


static int _zynqmp_getBasicGenerator(volatile u32 *reg, u8 *src, u8 *div0, u8 *div1, u8 *active)
{
	u32 val = *reg;
	*src = (u8)(val & 0x7U);
	*div0 = (u8)((val >> 8) & 0x3fU);
	*div1 = (u8)((val >> 16) & 0x3fU);
	*active = (u8)(val >> 24U);
	return 0;
}


int _zynqmp_getDevClock(u32 dev, u8 *src, u8 *div0, u8 *div1, u8 *active)
{
	if ((dev >= (unsigned int)pctl_devclock_lpd_usb3_dual) && (dev <= (unsigned int)pctl_devclock_lpd_timestamp)) {
		unsigned regOffset = (dev - (unsigned int)pctl_devclock_lpd_usb3_dual) + (unsigned int)crl_apb_usb3_dual_ref_ctrl;
		return _zynqmp_getBasicGenerator(zynq_common.crl_apb + regOffset, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		unsigned regOffset = (dev - (unsigned int)pctl_devclock_fpd_acpu) + (unsigned int)crf_apb_acpu_ctrl;
		return _zynqmp_getBasicGenerator(zynq_common.crf_apb + regOffset, src, div0, div1, active);
	}

	return -1;
}


static void _zynqmp_setMIOMuxing(unsigned pin, u8 l0, u8 l1, u8 l2, u8 l3)
{
	u32 val = ((l0 & 0x1UL) << 1) | ((l1 & 0x1UL) << 2) | ((l2 & 0x3UL) << 3) | ((l3 & 0x7UL) << 5);
	*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) = (*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & ~0xffU) | val;
}


static void _zynqmp_setMIOTristate(unsigned pin, u8 config)
{
	u32 reg = pin / 32U + (unsigned int)iou_slcr_mio_mst_tri0;
	u32 bit = pin % 32U;
	u32 mask = 0x01UL << bit;

	if ((config & PCTL_MIO_TRI_ENABLE) != 0U) {
		*(zynq_common.iou_slcr + reg) |= mask;
	}
	else {
		*(zynq_common.iou_slcr + reg) &= ~mask;
	}
}

static void _zynqmp_setMIOControl(unsigned pin, u8 config)
{
	u32 reg = (pin / 26U) * (unsigned int)(iou_slcr_bank1_ctrl0 - iou_slcr_bank0_ctrl0) + (unsigned int)iou_slcr_bank0_ctrl0;
	u32 bit = pin % 26U;
	u32 mask = 0x01UL << bit;
	unsigned int i;

	for (i = 0U; i <= 6U; i++) {
		if (i == 2U) {
			/* ctrl2 registers don't exist, skip */
			continue;
		}

		if ((config & (0x1U << i)) != 0U) {
			*(zynq_common.iou_slcr + reg + i) |= mask;
		}
		else {
			*(zynq_common.iou_slcr + reg + i) &= ~mask;
		}
	}
}


int _zynqmp_setMIO(unsigned pin, u8 l0, u8 l1, u8 l2, u8 l3, u8 config)
{
	if (pin > 77U) {
		return -1;
	}

	_zynqmp_setMIOMuxing(pin, l0, l1, l2, l3);
	_zynqmp_setMIOTristate(pin, config);
	_zynqmp_setMIOControl(pin, config);

	return 0;
}


static void _zynqmp_getMIOMuxing(unsigned pin, u8 *l0, u8 *l1, u8 *l2, u8 *l3)
{
	u32 val = *(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & 0xffU;
	*l0 = (u8)((val >> 1) & 0x1U);
	*l1 = (u8)((val >> 2) & 0x1U);
	*l2 = (u8)((val >> 3) & 0x3U);
	*l3 = (u8)((val >> 5) & 0x7U);
}


static void _zynqmp_getMIOTristate(unsigned pin, u8 *config)
{
	u32 reg = pin / 32U + (unsigned int)iou_slcr_mio_mst_tri0;
	u32 bit = pin % 32U;
	if ((*(zynq_common.iou_slcr + reg) & (0x01U << bit)) != 0U) {
		*config |= PCTL_MIO_TRI_ENABLE;
	}
}

static void _zynqmp_getMIOControl(unsigned pin, u8 *config)
{
	u32 reg = (pin / 26U) * (unsigned int)(iou_slcr_bank1_ctrl0 - iou_slcr_bank0_ctrl0) + (unsigned int)iou_slcr_bank0_ctrl0;
	u32 bit = pin % 26U;
	u32 mask = 0x01UL << bit;
	unsigned int i;

	for (i = 0U; i <= 6U; i++) {
		if (i == 2U) {
			/* ctrl2 registers don't exist, skip */
			continue;
		}

		if ((*(zynq_common.iou_slcr + reg + i) & mask) != 0U) {
			*config |= (0x1U << i);
		}
	}
}


static int _zynqmp_getMIO(unsigned pin, u8 *l0, u8 *l1, u8 *l2, u8 *l3, u8 *config)
{
	if (pin > 77U) {
		return -1;
	}

	*config = 0U;
	_zynqmp_getMIOMuxing(pin, l0, l1, l2, l3);
	_zynqmp_getMIOTristate(pin, config);
	_zynqmp_getMIOControl(pin, config);

	return 0;
}


static int _zynqmp_parseReset(int dev, volatile u32 **reg, u32 *bit)
{
	static const u32 lookup[] = {
		[pctl_devreset_lpd_gem0] = (unsigned)crl_apb_rst_lpd_iou0 | (0UL << 12),
		[pctl_devreset_lpd_gem1] = (unsigned)crl_apb_rst_lpd_iou0 | (1UL << 12),
		[pctl_devreset_lpd_gem2] = (unsigned)crl_apb_rst_lpd_iou0 | (2UL << 12),
		[pctl_devreset_lpd_gem3] = (unsigned)crl_apb_rst_lpd_iou0 | (3UL << 12),
		[pctl_devreset_lpd_qspi] = (unsigned)crl_apb_rst_lpd_iou2 | (0UL << 12),
		[pctl_devreset_lpd_uart0] = (unsigned)crl_apb_rst_lpd_iou2 | (1UL << 12),
		[pctl_devreset_lpd_uart1] = (unsigned)crl_apb_rst_lpd_iou2 | (2UL << 12),
		[pctl_devreset_lpd_spi0] = (unsigned)crl_apb_rst_lpd_iou2 | (3UL << 12),
		[pctl_devreset_lpd_spi1] = (unsigned)crl_apb_rst_lpd_iou2 | (4UL << 12),
		[pctl_devreset_lpd_sdio0] = (unsigned)crl_apb_rst_lpd_iou2 | (5UL << 12),
		[pctl_devreset_lpd_sdio1] = (unsigned)crl_apb_rst_lpd_iou2 | (6UL << 12),
		[pctl_devreset_lpd_can0] = (unsigned)crl_apb_rst_lpd_iou2 | (7UL << 12),
		[pctl_devreset_lpd_can1] = (unsigned)crl_apb_rst_lpd_iou2 | (8UL << 12),
		[pctl_devreset_lpd_i2c0] = (unsigned)crl_apb_rst_lpd_iou2 | (9UL << 12),
		[pctl_devreset_lpd_i2c1] = (unsigned)crl_apb_rst_lpd_iou2 | (10UL << 12),
		[pctl_devreset_lpd_ttc0] = (unsigned)crl_apb_rst_lpd_iou2 | (11UL << 12),
		[pctl_devreset_lpd_ttc1] = (unsigned)crl_apb_rst_lpd_iou2 | (12UL << 12),
		[pctl_devreset_lpd_ttc2] = (unsigned)crl_apb_rst_lpd_iou2 | (13UL << 12),
		[pctl_devreset_lpd_ttc3] = (unsigned)crl_apb_rst_lpd_iou2 | (14UL << 12),
		[pctl_devreset_lpd_swdt] = (unsigned)crl_apb_rst_lpd_iou2 | (15UL << 12),
		[pctl_devreset_lpd_nand] = (unsigned)crl_apb_rst_lpd_iou2 | (16UL << 12),
		[pctl_devreset_lpd_lpd_dma] = (unsigned)crl_apb_rst_lpd_iou2 | (17UL << 12),
		[pctl_devreset_lpd_gpio] = (unsigned)crl_apb_rst_lpd_iou2 | (18UL << 12),
		[pctl_devreset_lpd_iou_cc] = (unsigned)crl_apb_rst_lpd_iou2 | (19UL << 12),
		[pctl_devreset_lpd_timestamp] = (unsigned)crl_apb_rst_lpd_iou2 | (20UL << 12),
		[pctl_devreset_lpd_rpu_r50] = (unsigned)crl_apb_rst_lpd_top | (0UL << 12),
		[pctl_devreset_lpd_rpu_r51] = (unsigned)crl_apb_rst_lpd_top | (1UL << 12),
		[pctl_devreset_lpd_rpu_amba] = (unsigned)crl_apb_rst_lpd_top | (2UL << 12),
		[pctl_devreset_lpd_ocm] = (unsigned)crl_apb_rst_lpd_top | (3UL << 12),
		[pctl_devreset_lpd_rpu_pge] = (unsigned)crl_apb_rst_lpd_top | (4UL << 12),
		[pctl_devreset_lpd_usb0_corereset] = (unsigned)crl_apb_rst_lpd_top | (6UL << 12),
		[pctl_devreset_lpd_usb1_corereset] = (unsigned)crl_apb_rst_lpd_top | (7UL << 12),
		[pctl_devreset_lpd_usb0_hiberreset] = (unsigned)crl_apb_rst_lpd_top | (8UL << 12),
		[pctl_devreset_lpd_usb1_hiberreset] = (unsigned)crl_apb_rst_lpd_top | (9UL << 12),
		[pctl_devreset_lpd_usb0_apb] = (unsigned)crl_apb_rst_lpd_top | (10UL << 12),
		[pctl_devreset_lpd_usb1_apb] = (unsigned)crl_apb_rst_lpd_top | (11UL << 12),
		[pctl_devreset_lpd_ipi] = (unsigned)crl_apb_rst_lpd_top | (14UL << 12),
		[pctl_devreset_lpd_apm] = (unsigned)crl_apb_rst_lpd_top | (15UL << 12),
		[pctl_devreset_lpd_rtc] = (unsigned)crl_apb_rst_lpd_top | (16UL << 12),
		[pctl_devreset_lpd_sysmon] = (unsigned)crl_apb_rst_lpd_top | (17UL << 12),
		[pctl_devreset_lpd_s_axi_lpd] = (unsigned)crl_apb_rst_lpd_top | (19UL << 12),
		[pctl_devreset_lpd_lpd_swdt] = (unsigned)crl_apb_rst_lpd_top | (20UL << 12),
		[pctl_devreset_lpd_fpd] = (unsigned)crl_apb_rst_lpd_top | (23UL << 12),
		[pctl_devreset_lpd_dbg_fpd] = (unsigned)crl_apb_rst_lpd_dbg | (0UL << 12),
		[pctl_devreset_lpd_dbg_lpd] = (unsigned)crl_apb_rst_lpd_dbg | (1UL << 12),
		[pctl_devreset_lpd_rpu_dbg0] = (unsigned)crl_apb_rst_lpd_dbg | (4UL << 12),
		[pctl_devreset_lpd_rpu_dbg1] = (unsigned)crl_apb_rst_lpd_dbg | (5UL << 12),
		[pctl_devreset_lpd_dbg_ack] = (unsigned)crl_apb_rst_lpd_dbg | (15UL << 12),
		[pctl_devreset_fpd_sata] = (unsigned)crf_apb_rst_fpd_top | (1UL << 12),
		[pctl_devreset_fpd_gt] = (unsigned)crf_apb_rst_fpd_top | (2UL << 12),
		[pctl_devreset_fpd_gpu] = (unsigned)crf_apb_rst_fpd_top | (3UL << 12),
		[pctl_devreset_fpd_gpu_pp0] = (unsigned)crf_apb_rst_fpd_top | (4UL << 12),
		[pctl_devreset_fpd_gpu_pp1] = (unsigned)crf_apb_rst_fpd_top | (5UL << 12),
		[pctl_devreset_fpd_fpd_dma] = (unsigned)crf_apb_rst_fpd_top | (6UL << 12),
		[pctl_devreset_fpd_s_axi_hpc_0_fpd] = (unsigned)crf_apb_rst_fpd_top | (7UL << 12),
		[pctl_devreset_fpd_s_axi_hpc_1_fpd] = (unsigned)crf_apb_rst_fpd_top | (8UL << 12),
		[pctl_devreset_fpd_s_axi_hp_0_fpd] = (unsigned)crf_apb_rst_fpd_top | (9UL << 12),
		[pctl_devreset_fpd_s_axi_hp_1_fpd] = (unsigned)crf_apb_rst_fpd_top | (10UL << 12),
		[pctl_devreset_fpd_s_axi_hpc_2_fpd] = (unsigned)crf_apb_rst_fpd_top | (11UL << 12),
		[pctl_devreset_fpd_s_axi_hpc_3_fpd] = (unsigned)crf_apb_rst_fpd_top | (12UL << 12),
		[pctl_devreset_fpd_swdt] = (unsigned)crf_apb_rst_fpd_top | (15UL << 12),
		[pctl_devreset_fpd_dp] = (unsigned)crf_apb_rst_fpd_top | (16UL << 12),
		[pctl_devreset_fpd_pcie_ctrl] = (unsigned)crf_apb_rst_fpd_top | (17UL << 12),
		[pctl_devreset_fpd_pcie_bridge] = (unsigned)crf_apb_rst_fpd_top | (18UL << 12),
		[pctl_devreset_fpd_pcie_cfg] = (unsigned)crf_apb_rst_fpd_top | (19UL << 12),
		[pctl_devreset_fpd_acpu0] = (unsigned)crf_apb_rst_fpd_apu | (0UL << 12),
		[pctl_devreset_fpd_acpu1] = (unsigned)crf_apb_rst_fpd_apu | (1UL << 12),
		[pctl_devreset_fpd_acpu2] = (unsigned)crf_apb_rst_fpd_apu | (2UL << 12),
		[pctl_devreset_fpd_acpu3] = (unsigned)crf_apb_rst_fpd_apu | (3UL << 12),
		[pctl_devreset_fpd_apu_l2] = (unsigned)crf_apb_rst_fpd_apu | (8UL << 12),
		[pctl_devreset_fpd_acpu0_pwron] = (unsigned)crf_apb_rst_fpd_apu | (10UL << 12),
		[pctl_devreset_fpd_acpu1_pwron] = (unsigned)crf_apb_rst_fpd_apu | (11UL << 12),
		[pctl_devreset_fpd_acpu2_pwron] = (unsigned)crf_apb_rst_fpd_apu | (12UL << 12),
		[pctl_devreset_fpd_acpu3_pwron] = (unsigned)crf_apb_rst_fpd_apu | (13UL << 12),
		[pctl_devreset_fpd_ddr_apm] = (unsigned)crf_apb_rst_ddr_ss | (2UL << 12),
		[pctl_devreset_fpd_ddr_reserved] = (unsigned)crf_apb_rst_ddr_ss | (3UL << 12),
	};

	if ((dev < pctl_devreset_lpd_gem0) || (dev > pctl_devreset_fpd_ddr_reserved)) {
		return -1;
	}

	if (dev >= pctl_devreset_fpd_sata) {
		*reg = zynq_common.crf_apb + (lookup[dev] & ((0x1UL << 12) - 1U));
	}
	else {
		*reg = zynq_common.crl_apb + (lookup[dev] & ((0x1UL << 12) - 1U));
	}

	*bit = (0x1UL << (lookup[dev] >> 12U));
	return 0;
}


int _zynq_setDevRst(int dev, unsigned int state)
{
	volatile u32 *reg;
	u32 bit;

	if (_zynqmp_parseReset(dev, &reg, &bit) < 0) {
		return -1;
	}

	if (state != 0U) {
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

	*state = ((*reg & bit) != 0U) ? 1 : 0;
	return 0;
}


static void zynqmp_softRst(void)
{
	/* Equivalent to PS_SRST_B signal */
	*(zynq_common.crl_apb + crl_apb_reset_ctrl) |= (1UL << 4);
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
			if (data->action == pctl_set) {
				ret = _zynqmp_setDevClock(data->devclock.dev, data->devclock.src, data->devclock.div0, data->devclock.div1, data->devclock.active);
			}
			else if (data->action == pctl_set) {
				ret = _zynqmp_getDevClock(data->devclock.dev, &data->devclock.src, &data->devclock.div0, &data->devclock.div1, &data->devclock.active);
			}
			break;

		case pctl_mio:
			if (data->action == pctl_set) {
				ret = _zynqmp_setMIO(data->mio.pin, data->mio.l0, data->mio.l1, data->mio.l2, data->mio.l3, data->mio.config);
			}
			else if (data->action == pctl_get) {
				ret = _zynqmp_getMIO(data->mio.pin, &data->mio.l0, &data->mio.l1, &data->mio.l2, &data->mio.l3, &data->mio.config);
			}
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
	zynq_common.iou_slcr = (void *)IOU_SLCR_BASE_ADDRESS;
	zynq_common.crf_apb = (void *)CRF_APB_BASE_ADDRESS;
	zynq_common.crl_apb = (void *)CRL_APB_BASE_ADDRESS;
}


unsigned int hal_cpuGetCount(void)
{
	return 1;
}
