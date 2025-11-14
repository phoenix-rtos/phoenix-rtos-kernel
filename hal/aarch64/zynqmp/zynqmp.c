/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ZynqMP internal peripheral control functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/hal.h"
#include "hal/aarch64/aarch64.h"
#include "hal/aarch64/interrupts_gicv2.h"
#include "hal/spinlock.h"
#include "include/arch/aarch64/zynqmp/zynqmp.h"

#include "hal/aarch64/halsyspage.h"
#include "hal/aarch64/arch/pmap.h"
#include "zynqmp_regs.h"

#define IOU_SLCR_BASE_ADDRESS 0xff180000U
#define APU_BASE_ADDRESS      0xfd5c0000U
#define CRF_APB_BASE_ADDRESS  0xfd1a0000U
#define CRL_APB_BASE_ADDRESS  0xff5e0000U

static struct {
	volatile u32 *iou_slcr;
	volatile u32 *apu;
	volatile u32 *crf_apb;
	volatile u32 *crl_apb;
	spinlock_t pltctlSp;
	unsigned int nCpus;
} zynq_common;


/* parasoft-begin-suppress MISRAC2012-RULE_8_4 "Definition in assembly" */
volatile unsigned int nCpusStarted = 0U;


int _interrupts_gicv2_classify(unsigned int irqn)
{
	/* ZynqMP specific: most interrupts are high level, some are reserved.
	 * PL to PS interrupts can be either high level or rising edge, here we configure
	 * lower half as high level, upper half as rising edge */
	if ((irqn < 40U) || ((irqn >= 129U) && (irqn <= 135U))) {
		return gicv2_cfg_reserved;
	}
	else if ((irqn >= 136U) && (irqn <= 143U)) {
		return gicv2_cfg_rising_edge;
	}
	else {
		return gicv2_cfg_high_level;
	}
}


static unsigned int _zynqmp_getActiveBitShift(int dev)
{
	if ((dev >= pctl_devclock_lpd_usb3_dual) && (dev <= pctl_devclock_lpd_usb1_bus)) {
		return 25U;
	}
	else {
		return 24U;
	}
}


static int _zynqmp_setBasicGenerator(volatile u32 *reg, int dev, u8 src, u8 div0, u8 div1, u8 active)
{
	u32 val = (u32)src;
	if (dev == pctl_devclock_lpd_timestamp) {
		val &= 0x7U;
	}
	else {
		val &= 0x3U;
	}

	val |= (((u32)div0 & 0x3fU) << 8) | (((u32)div1 & 0x3fU) << 16) | ((u32)active << _zynqmp_getActiveBitShift(dev));
	if (dev == pctl_devclock_lpd_cpu_r5) {
		/* According to docs turning this bit off could lead to system hang - ensure it is on */
		val |= ((u32)1U << 24);
	}

	*reg = val;
	hal_cpuDataSyncBarrier();
	return 0;
}


static int _zynqmp_setDevClock(int dev, u8 src, u8 div0, u8 div1, u8 active)
{
	u32 regOffset;

	if ((dev >= pctl_devclock_lpd_usb3_dual) && (dev <= pctl_devclock_lpd_timestamp)) {
		regOffset = (u32)(s32)((dev - pctl_devclock_lpd_usb3_dual) + crl_apb_usb3_dual_ref_ctrl);
		return _zynqmp_setBasicGenerator(zynq_common.crl_apb + regOffset, dev, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		regOffset = (u32)(s32)((dev - pctl_devclock_fpd_acpu) + crf_apb_acpu_ctrl);
		return _zynqmp_setBasicGenerator(zynq_common.crf_apb + regOffset, dev, src, div0, div1, active);
	}
	else {
		/* No action required */
	}

	return -1;
}


static int _zynqmp_getBasicGenerator(int dev, volatile u32 *reg, u8 *src, u8 *div0, u8 *div1, u8 *active)
{
	u32 val = *reg;
	*src = (u8)(val & 0x7U);
	*div0 = (u8)((val >> 8) & 0x3fU);
	*div1 = (u8)((val >> 16) & 0x3fU);
	*active = (u8)(val >> _zynqmp_getActiveBitShift(dev));
	return 0;
}


int _zynqmp_getDevClock(int dev, u8 *src, u8 *div0, u8 *div1, u8 *active)
{
	u32 regOffset;

	if ((dev >= pctl_devclock_lpd_usb3_dual) && (dev <= pctl_devclock_lpd_timestamp)) {
		regOffset = (u32)(s32)((dev - pctl_devclock_lpd_usb3_dual) + crl_apb_usb3_dual_ref_ctrl);
		return _zynqmp_getBasicGenerator(dev, zynq_common.crl_apb + regOffset, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		regOffset = (u32)(s32)((dev - pctl_devclock_fpd_acpu) + crf_apb_acpu_ctrl);
		return _zynqmp_getBasicGenerator(dev, zynq_common.crf_apb + regOffset, src, div0, div1, active);
	}
	else {
		/* No action required */
	}

	return -1;
}


static void _zynqmp_setMIOMuxing(unsigned int pin, u8 l0, u8 l1, u8 l2, u8 l3)
{
	u32 val = (((u32)l0 & 0x1U) << 1) | (((u32)l1 & 0x1U) << 2) | (((u32)l2 & 0x3U) << 3) | (((u32)l3 & 0x7U) << 5);
	*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) = (*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & ~0xffU) | val;
}


static void _zynqmp_setMIOTristate(unsigned int pin, u8 config)
{
	u32 reg = pin / 32U + (u32)iou_slcr_mio_mst_tri0;
	u32 bit = pin % 32U;
	u32 mask = (u32)1U << bit;

	if ((config & PCTL_MIO_TRI_ENABLE) != 0U) {
		*(zynq_common.iou_slcr + reg) |= mask;
	}
	else {
		*(zynq_common.iou_slcr + reg) &= ~mask;
	}
}

static void _zynqmp_setMIOControl(unsigned int pin, u8 config)
{
	u32 reg = (pin / 26U) * (unsigned int)(iou_slcr_bank1_ctrl0 - iou_slcr_bank0_ctrl0) + (unsigned int)iou_slcr_bank0_ctrl0;
	u32 bit = pin % 26U;
	u32 mask = (u32)1U << bit;
	unsigned int i;

	for (i = 0U; i <= 6U; i++) {
		if (i == 2U) {
			/* ctrl2 registers don't exist, skip */
			continue;
		}

		if ((config & (1U << i)) != 0U) {
			*(zynq_common.iou_slcr + reg + i) |= mask;
		}
		else {
			*(zynq_common.iou_slcr + reg + i) &= ~mask;
		}
	}
}


int _zynqmp_setMIO(unsigned int pin, u8 l0, u8 l1, u8 l2, u8 l3, u8 config)
{
	if (pin > 77U) {
		return -1;
	}

	_zynqmp_setMIOMuxing(pin, l0, l1, l2, l3);
	_zynqmp_setMIOTristate(pin, config);
	_zynqmp_setMIOControl(pin, config);

	return 0;
}


static void _zynqmp_getMIOMuxing(unsigned int pin, u8 *l0, u8 *l1, u8 *l2, u8 *l3)
{
	u32 val = *(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & 0xffU;
	*l0 = (u8)(val >> 1) & 0x1U;
	*l1 = (u8)(val >> 2) & 0x1U;
	*l2 = (u8)(val >> 3) & 0x3U;
	*l3 = (u8)(val >> 5) & 0x7U;
}


static void _zynqmp_getMIOTristate(unsigned int pin, u8 *config)
{
	u32 reg = pin / 32U + (u32)iou_slcr_mio_mst_tri0;
	u32 bit = pin % 32U;
	if ((*(zynq_common.iou_slcr + reg) & (1UL << bit)) != 0U) {
		*config |= PCTL_MIO_TRI_ENABLE;
	}
}

static void _zynqmp_getMIOControl(unsigned int pin, u8 *config)
{
	u32 reg = (pin / 26U) * (u32)(iou_slcr_bank1_ctrl0 - iou_slcr_bank0_ctrl0) + (u32)iou_slcr_bank0_ctrl0;
	u32 bit = pin % 26U;
	u32 mask = (u32)1U << bit;
	unsigned int i;

	for (i = 0U; i <= 6U; i++) {
		if (i == 2U) {
			/* ctrl2 registers don't exist, skip */
			continue;
		}

		if ((*(zynq_common.iou_slcr + reg + i) & mask) != 0U) {
			*config |= ((u8)1 << i);
		}
	}
}


static int _zynqmp_getMIO(unsigned int pin, u8 *l0, u8 *l1, u8 *l2, u8 *l3, u8 *config)
{
	if (pin > 77U) {
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
	static const u32 lookup[76] = {
		[pctl_devreset_lpd_gem0] = (u32)crl_apb_rst_lpd_iou0 | ((u32)0U << 12),
		[pctl_devreset_lpd_gem1] = (u32)crl_apb_rst_lpd_iou0 | ((u32)1U << 12),
		[pctl_devreset_lpd_gem2] = (u32)crl_apb_rst_lpd_iou0 | ((u32)2U << 12),
		[pctl_devreset_lpd_gem3] = (u32)crl_apb_rst_lpd_iou0 | ((u32)3U << 12),
		[pctl_devreset_lpd_qspi] = (u32)crl_apb_rst_lpd_iou2 | ((u32)0U << 12),
		[pctl_devreset_lpd_uart0] = (u32)crl_apb_rst_lpd_iou2 | ((u32)1U << 12),
		[pctl_devreset_lpd_uart1] = (u32)crl_apb_rst_lpd_iou2 | ((u32)2U << 12),
		[pctl_devreset_lpd_spi0] = (u32)crl_apb_rst_lpd_iou2 | ((u32)3U << 12),
		[pctl_devreset_lpd_spi1] = (u32)crl_apb_rst_lpd_iou2 | ((u32)4U << 12),
		[pctl_devreset_lpd_sdio0] = (u32)crl_apb_rst_lpd_iou2 | ((u32)5U << 12),
		[pctl_devreset_lpd_sdio1] = (u32)crl_apb_rst_lpd_iou2 | ((u32)6U << 12),
		[pctl_devreset_lpd_can0] = (u32)crl_apb_rst_lpd_iou2 | ((u32)7U << 12),
		[pctl_devreset_lpd_can1] = (u32)crl_apb_rst_lpd_iou2 | ((u32)8U << 12),
		[pctl_devreset_lpd_i2c0] = (u32)crl_apb_rst_lpd_iou2 | ((u32)9U << 12),
		[pctl_devreset_lpd_i2c1] = (u32)crl_apb_rst_lpd_iou2 | ((u32)10U << 12),
		[pctl_devreset_lpd_ttc0] = (u32)crl_apb_rst_lpd_iou2 | ((u32)11U << 12),
		[pctl_devreset_lpd_ttc1] = (u32)crl_apb_rst_lpd_iou2 | ((u32)12U << 12),
		[pctl_devreset_lpd_ttc2] = (u32)crl_apb_rst_lpd_iou2 | ((u32)13U << 12),
		[pctl_devreset_lpd_ttc3] = (u32)crl_apb_rst_lpd_iou2 | ((u32)14U << 12),
		[pctl_devreset_lpd_swdt] = (u32)crl_apb_rst_lpd_iou2 | ((u32)15U << 12),
		[pctl_devreset_lpd_nand] = (u32)crl_apb_rst_lpd_iou2 | ((u32)16U << 12),
		[pctl_devreset_lpd_lpd_dma] = (u32)crl_apb_rst_lpd_iou2 | ((u32)17U << 12),
		[pctl_devreset_lpd_gpio] = (u32)crl_apb_rst_lpd_iou2 | ((u32)18U << 12),
		[pctl_devreset_lpd_iou_cc] = (u32)crl_apb_rst_lpd_iou2 | ((u32)19U << 12),
		[pctl_devreset_lpd_timestamp] = (u32)crl_apb_rst_lpd_iou2 | ((u32)20U << 12),
		[pctl_devreset_lpd_rpu_r50] = (u32)crl_apb_rst_lpd_top | ((u32)0U << 12),
		[pctl_devreset_lpd_rpu_r51] = (u32)crl_apb_rst_lpd_top | ((u32)1U << 12),
		[pctl_devreset_lpd_rpu_amba] = (u32)crl_apb_rst_lpd_top | ((u32)2U << 12),
		[pctl_devreset_lpd_ocm] = (u32)crl_apb_rst_lpd_top | ((u32)3U << 12),
		[pctl_devreset_lpd_rpu_pge] = (u32)crl_apb_rst_lpd_top | ((u32)4U << 12),
		[pctl_devreset_lpd_usb0_corereset] = (u32)crl_apb_rst_lpd_top | ((u32)6U << 12),
		[pctl_devreset_lpd_usb1_corereset] = (u32)crl_apb_rst_lpd_top | ((u32)7U << 12),
		[pctl_devreset_lpd_usb0_hiberreset] = (u32)crl_apb_rst_lpd_top | ((u32)8U << 12),
		[pctl_devreset_lpd_usb1_hiberreset] = (u32)crl_apb_rst_lpd_top | ((u32)9U << 12),
		[pctl_devreset_lpd_usb0_apb] = (u32)crl_apb_rst_lpd_top | ((u32)10U << 12),
		[pctl_devreset_lpd_usb1_apb] = (u32)crl_apb_rst_lpd_top | ((u32)11U << 12),
		[pctl_devreset_lpd_ipi] = (u32)crl_apb_rst_lpd_top | ((u32)14U << 12),
		[pctl_devreset_lpd_apm] = (u32)crl_apb_rst_lpd_top | ((u32)15U << 12),
		[pctl_devreset_lpd_rtc] = (u32)crl_apb_rst_lpd_top | ((u32)16U << 12),
		[pctl_devreset_lpd_sysmon] = (u32)crl_apb_rst_lpd_top | ((u32)17U << 12),
		[pctl_devreset_lpd_s_axi_lpd] = (u32)crl_apb_rst_lpd_top | ((u32)19U << 12),
		[pctl_devreset_lpd_lpd_swdt] = (u32)crl_apb_rst_lpd_top | ((u32)20U << 12),
		[pctl_devreset_lpd_fpd] = (u32)crl_apb_rst_lpd_top | ((u32)23U << 12),
		[pctl_devreset_lpd_dbg_fpd] = (u32)crl_apb_rst_lpd_dbg | ((u32)0U << 12),
		[pctl_devreset_lpd_dbg_lpd] = (u32)crl_apb_rst_lpd_dbg | ((u32)1U << 12),
		[pctl_devreset_lpd_rpu_dbg0] = (u32)crl_apb_rst_lpd_dbg | ((u32)4U << 12),
		[pctl_devreset_lpd_rpu_dbg1] = (u32)crl_apb_rst_lpd_dbg | ((u32)5U << 12),
		[pctl_devreset_lpd_dbg_ack] = (u32)crl_apb_rst_lpd_dbg | ((u32)15U << 12),
		[pctl_devreset_fpd_sata] = (u32)crf_apb_rst_fpd_top | ((u32)1U << 12),
		[pctl_devreset_fpd_gt] = (u32)crf_apb_rst_fpd_top | ((u32)2U << 12),
		[pctl_devreset_fpd_gpu] = (u32)crf_apb_rst_fpd_top | ((u32)3U << 12),
		[pctl_devreset_fpd_gpu_pp0] = (u32)crf_apb_rst_fpd_top | ((u32)4U << 12),
		[pctl_devreset_fpd_gpu_pp1] = (u32)crf_apb_rst_fpd_top | ((u32)5U << 12),
		[pctl_devreset_fpd_fpd_dma] = (u32)crf_apb_rst_fpd_top | ((u32)6U << 12),
		[pctl_devreset_fpd_s_axi_hpc_0_fpd] = (u32)crf_apb_rst_fpd_top | ((u32)7U << 12),
		[pctl_devreset_fpd_s_axi_hpc_1_fpd] = (u32)crf_apb_rst_fpd_top | ((u32)8U << 12),
		[pctl_devreset_fpd_s_axi_hp_0_fpd] = (u32)crf_apb_rst_fpd_top | ((u32)9U << 12),
		[pctl_devreset_fpd_s_axi_hp_1_fpd] = (u32)crf_apb_rst_fpd_top | ((u32)10U << 12),
		[pctl_devreset_fpd_s_axi_hpc_2_fpd] = (u32)crf_apb_rst_fpd_top | ((u32)11U << 12),
		[pctl_devreset_fpd_s_axi_hpc_3_fpd] = (u32)crf_apb_rst_fpd_top | ((u32)12U << 12),
		[pctl_devreset_fpd_swdt] = (u32)crf_apb_rst_fpd_top | ((u32)15U << 12),
		[pctl_devreset_fpd_dp] = (u32)crf_apb_rst_fpd_top | ((u32)16U << 12),
		[pctl_devreset_fpd_pcie_ctrl] = (u32)crf_apb_rst_fpd_top | ((u32)17U << 12),
		[pctl_devreset_fpd_pcie_bridge] = (u32)crf_apb_rst_fpd_top | ((u32)18U << 12),
		[pctl_devreset_fpd_pcie_cfg] = (u32)crf_apb_rst_fpd_top | ((u32)19U << 12),
		[pctl_devreset_fpd_acpu0] = (u32)crf_apb_rst_fpd_apu | ((u32)0U << 12),
		[pctl_devreset_fpd_acpu1] = (u32)crf_apb_rst_fpd_apu | ((u32)1U << 12),
		[pctl_devreset_fpd_acpu2] = (u32)crf_apb_rst_fpd_apu | ((u32)2U << 12),
		[pctl_devreset_fpd_acpu3] = (u32)crf_apb_rst_fpd_apu | ((u32)3U << 12),
		[pctl_devreset_fpd_apu_l2] = (u32)crf_apb_rst_fpd_apu | ((u32)8U << 12),
		[pctl_devreset_fpd_acpu0_pwron] = (u32)crf_apb_rst_fpd_apu | ((u32)10U << 12),
		[pctl_devreset_fpd_acpu1_pwron] = (u32)crf_apb_rst_fpd_apu | ((u32)11U << 12),
		[pctl_devreset_fpd_acpu2_pwron] = (u32)crf_apb_rst_fpd_apu | ((u32)12U << 12),
		[pctl_devreset_fpd_acpu3_pwron] = (u32)crf_apb_rst_fpd_apu | ((u32)13U << 12),
		[pctl_devreset_fpd_ddr_apm] = (u32)crf_apb_rst_ddr_ss | ((u32)2U << 12),
		[pctl_devreset_fpd_ddr_reserved] = (u32)crf_apb_rst_ddr_ss | ((u32)3U << 12),
	};

	if ((dev < pctl_devreset_lpd_gem0) || (dev > pctl_devreset_fpd_ddr_reserved)) {
		return -1;
	}

	if (dev >= pctl_devreset_fpd_sata) {
		*reg = zynq_common.crf_apb + (lookup[dev] & ((1UL << 12) - 1U));
	}
	else {
		*reg = zynq_common.crl_apb + (lookup[dev] & ((1UL << 12) - 1U));
	}

	*bit = ((u32)1U << (lookup[dev] >> 12));
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


__attribute__((noreturn)) static void zynqmp_softRst(void)
{
	/* Equivalent to PS_SRST_B signal */
	*(zynq_common.crl_apb + crl_apb_reset_ctrl) |= (1U << 4);
	__builtin_unreachable();
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
	u8 src, div0, div1, active;


	hal_spinlockSet(&zynq_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_devclock:
			if (data->action == pctl_set) {
				ret = _zynqmp_setDevClock(data->devclock.dev, data->devclock.src, data->devclock.div0, data->devclock.div1, data->devclock.active);
			}
			else if (data->action == pctl_set) {
				ret = _zynqmp_getDevClock(data->devclock.dev, &src, &div0, &div1, &active);
				data->devclock.src = src;
				data->devclock.div0 = div0;
				data->devclock.div1 = div1;
				data->devclock.active = active;
			}
			else {
				/* No action required */
			}
			break;

		case pctl_mio:
			if (data->action == pctl_set) {
				ret = _zynqmp_setMIO(data->mio.pin, data->mio.l0, data->mio.l1, data->mio.l2, data->mio.l3, data->mio.config);
			}
			else if (data->action == pctl_get) {
				ret = _zynqmp_getMIO(data->mio.pin, &data->mio.l0, &data->mio.l1, &data->mio.l2, &data->mio.l3, &data->mio.config);
			}
			else {
				/* No action required */
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
			else {
				/* No action required */
			}
			break;

		case pctl_reboot:
			if ((data->action == pctl_set) && (data->reboot.magic == PCTL_REBOOT_MAGIC)) {
				zynqmp_softRst();
			}
			else if (data->action == pctl_get) {
				ret = 0;
				data->reboot.reason = (unsigned int)hal_syspage->hs.resetReason;
			}
			else {
				/* No action required */
			}
			break;

		default:
			/* No action required */
			break;
	}

	hal_spinlockClear(&zynq_common.pltctlSp, &sc);

	return ret;
}


static u32 hal_checkNumCPUs(void)
{
	int i;
	u32 powerStatus, cpusAvailable;
	/* First check if MPIDR indicates uniprocessor system or no MP extensions */
	u64 mpidr = sysreg_read(mpidr_el1);
	if (((mpidr >> 30) & 0x2U) != 0x2U) {
		return 1;
	}

	powerStatus = (~*(zynq_common.crf_apb + crf_apb_rst_fpd_apu)) & 0xfU;
	cpusAvailable = 0;
	for (i = 0; i < 4; i++) {
		if ((powerStatus & 0x1U) == 1U) {
			cpusAvailable++;
		}
		powerStatus >>= 1;
	}

	return cpusAvailable;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&zynq_common.pltctlSp, "pltctl");
	zynq_common.iou_slcr = _pmap_halMapDevice(IOU_SLCR_BASE_ADDRESS, 0, SIZE_PAGE);
	zynq_common.crf_apb = _pmap_halMapDevice(CRF_APB_BASE_ADDRESS, 0, SIZE_PAGE);
	zynq_common.crl_apb = _pmap_halMapDevice(CRL_APB_BASE_ADDRESS, 0, SIZE_PAGE);
	zynq_common.apu = _pmap_halMapDevice(APU_BASE_ADDRESS, 0, SIZE_PAGE);
	zynq_common.nCpus = hal_checkNumCPUs();
}


unsigned int hal_cpuGetCount(void)
{
	return zynq_common.nCpus;
}


void _hal_cpuInit(void)
{
	hal_cpuAtomicInc(&nCpusStarted);

	hal_cpuSignalEvent();
	while (hal_cpuAtomicGet(&nCpusStarted) != zynq_common.nCpus) {
		hal_cpuWaitForEvent();
	}
}


void hal_cpuSmpSync(void)
{
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}
