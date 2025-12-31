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
#include "hal/spinlock.h"
#include "zynqmp.h"

#include "hal/platform/zynq/timer_ttc_impl.h"
#include "hal/aarch64/halsyspage.h"
#include "hal/aarch64/arch/pmap.h"
#include "zynqmp_regs.h"

#define TTC0_BASE_ADDR        0xff110000UL
#define IOU_SLCR_BASE_ADDRESS 0xff180000UL
#define APU_BASE_ADDRESS      0xfd5c0000UL
#define CRF_APB_BASE_ADDRESS  0xfd1a0000UL
#define CRL_APB_BASE_ADDRESS  0xff5e0000UL


/* PLO entrypoint */
void _start(void);

struct {
	volatile u32 *iou_slcr;
	volatile u32 *apu;
	volatile u32 *crf_apb;
	volatile u32 *crl_apb;
	spinlock_t pltctlSp;
	unsigned int nCpus;
} zynq_common;


volatile unsigned int nCpusStarted = 0;


int _interrupts_gicv2_classify(unsigned int irqn)
{
	/* ZynqMP specific: most interrupts are high level, some are reserved.
	 * PL to PS interrupts can be either high level or rising edge, here we configure
	 * lower half as high level, upper half as rising edge */
	if ((irqn < 40) || ((irqn >= 129) && (irqn <= 135))) {
		return gicv2_cfg_reserved;
	}
	else if ((irqn >= 136) && (irqn <= 143)) {
		return gicv2_cfg_rising_edge;
	}
	else {
		return gicv2_cfg_high_level;
	}
}


static int _zynqmp_getActiveBitShift(int dev)
{
	if ((dev >= pctl_devclock_lpd_usb3_dual) && (dev <= pctl_devclock_lpd_usb1_bus)) {
		return 25;
	}
	else {
		return 24;
	}
}


static int _zynqmp_setBasicGenerator(volatile u32 *reg, int dev, char src, char div0, char div1, char active)
{
	u32 val = src;
	if (dev == pctl_devclock_lpd_timestamp) {
		val &= 0x7;
	}
	else {
		val &= 0x3;
	}

	val |= ((div0 & 0x3f) << 8) | ((div1 & 0x3f) << 16) | (active << _zynqmp_getActiveBitShift(dev));
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
		unsigned int regOffset = (dev - pctl_devclock_lpd_usb3_dual) + crl_apb_usb3_dual_ref_ctrl;
		return _zynqmp_setBasicGenerator(zynq_common.crl_apb + regOffset, dev, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		unsigned int regOffset = (dev - pctl_devclock_fpd_acpu) + crf_apb_acpu_ctrl;
		return _zynqmp_setBasicGenerator(zynq_common.crf_apb + regOffset, dev, src, div0, div1, active);
	}

	return -1;
}


static int _zynqmp_getBasicGenerator(int dev, volatile u32 *reg, char *src, char *div0, char *div1, char *active)
{
	u32 val = *reg;
	*src = val & 0x7;
	*div0 = (val >> 8) & 0x3f;
	*div1 = (val >> 16) & 0x3f;
	*active = val >> _zynqmp_getActiveBitShift(dev);
	return 0;
}


int _zynqmp_getDevClock(int dev, char *src, char *div0, char *div1, char *active)
{
	if ((dev >= pctl_devclock_lpd_usb3_dual) && (dev <= pctl_devclock_lpd_timestamp)) {
		unsigned int regOffset = (dev - pctl_devclock_lpd_usb3_dual) + crl_apb_usb3_dual_ref_ctrl;
		return _zynqmp_getBasicGenerator(dev, zynq_common.crl_apb + regOffset, src, div0, div1, active);
	}
	else if ((dev >= pctl_devclock_fpd_acpu) && (dev <= pctl_devclock_fpd_dbg_tstmp)) {
		unsigned int regOffset = (dev - pctl_devclock_fpd_acpu) + crf_apb_acpu_ctrl;
		return _zynqmp_getBasicGenerator(dev, zynq_common.crf_apb + regOffset, src, div0, div1, active);
	}

	return -1;
}


static void _zynqmp_setMIOMuxing(unsigned int pin, u8 l0, u8 l1, u8 l2, u8 l3)
{
	u32 val = ((l0 & 0x1) << 1) | ((l1 & 0x1) << 2) | ((l2 & 0x3) << 3) | ((l3 & 0x7) << 5);
	*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) = (*(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & ~0xff) | val;
}


static void _zynqmp_setMIOTristate(unsigned int pin, char config)
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

static void _zynqmp_setMIOControl(unsigned int pin, char config)
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


int _zynqmp_setMIO(unsigned int pin, u8 l0, u8 l1, u8 l2, u8 l3, u8 config)
{
	if (pin > 77) {
		return -1;
	}

	_zynqmp_setMIOMuxing(pin, l0, l1, l2, l3);
	_zynqmp_setMIOTristate(pin, config);
	_zynqmp_setMIOControl(pin, config);

	return 0;
}


static void _zynqmp_getMIOMuxing(unsigned int pin, u8 *l0, u8 *l1, u8 *l2, u8 *l3)
{
	u32 val = *(zynq_common.iou_slcr + iou_slcr_mio_pin_0 + pin) & 0xff;
	*l0 = (val >> 1) & 0x1;
	*l1 = (val >> 2) & 0x1;
	*l2 = (val >> 3) & 0x3;
	*l3 = (val >> 5) & 0x7;
}


static void _zynqmp_getMIOTristate(unsigned int pin, u8 *config)
{
	u32 reg = pin / 32 + iou_slcr_mio_mst_tri0;
	u32 bit = pin % 32;
	if (*(zynq_common.iou_slcr + reg) & (1 << bit)) {
		*config |= PCTL_MIO_TRI_ENABLE;
	}
}

static void _zynqmp_getMIOControl(unsigned int pin, u8 *config)
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


static int _zynqmp_getMIO(unsigned int pin, u8 *l0, u8 *l1, u8 *l2, u8 *l3, u8 *config)
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
	static const u32 lookup[] = {
		[pctl_devreset_lpd_gem0] = crl_apb_rst_lpd_iou0 | (0 << 12),
		[pctl_devreset_lpd_gem1] = crl_apb_rst_lpd_iou0 | (1 << 12),
		[pctl_devreset_lpd_gem2] = crl_apb_rst_lpd_iou0 | (2 << 12),
		[pctl_devreset_lpd_gem3] = crl_apb_rst_lpd_iou0 | (3 << 12),
		[pctl_devreset_lpd_qspi] = crl_apb_rst_lpd_iou2 | (0 << 12),
		[pctl_devreset_lpd_uart0] = crl_apb_rst_lpd_iou2 | (1 << 12),
		[pctl_devreset_lpd_uart1] = crl_apb_rst_lpd_iou2 | (2 << 12),
		[pctl_devreset_lpd_spi0] = crl_apb_rst_lpd_iou2 | (3 << 12),
		[pctl_devreset_lpd_spi1] = crl_apb_rst_lpd_iou2 | (4 << 12),
		[pctl_devreset_lpd_sdio0] = crl_apb_rst_lpd_iou2 | (5 << 12),
		[pctl_devreset_lpd_sdio1] = crl_apb_rst_lpd_iou2 | (6 << 12),
		[pctl_devreset_lpd_can0] = crl_apb_rst_lpd_iou2 | (7 << 12),
		[pctl_devreset_lpd_can1] = crl_apb_rst_lpd_iou2 | (8 << 12),
		[pctl_devreset_lpd_i2c0] = crl_apb_rst_lpd_iou2 | (9 << 12),
		[pctl_devreset_lpd_i2c1] = crl_apb_rst_lpd_iou2 | (10 << 12),
		[pctl_devreset_lpd_ttc0] = crl_apb_rst_lpd_iou2 | (11 << 12),
		[pctl_devreset_lpd_ttc1] = crl_apb_rst_lpd_iou2 | (12 << 12),
		[pctl_devreset_lpd_ttc2] = crl_apb_rst_lpd_iou2 | (13 << 12),
		[pctl_devreset_lpd_ttc3] = crl_apb_rst_lpd_iou2 | (14 << 12),
		[pctl_devreset_lpd_swdt] = crl_apb_rst_lpd_iou2 | (15 << 12),
		[pctl_devreset_lpd_nand] = crl_apb_rst_lpd_iou2 | (16 << 12),
		[pctl_devreset_lpd_lpd_dma] = crl_apb_rst_lpd_iou2 | (17 << 12),
		[pctl_devreset_lpd_gpio] = crl_apb_rst_lpd_iou2 | (18 << 12),
		[pctl_devreset_lpd_iou_cc] = crl_apb_rst_lpd_iou2 | (19 << 12),
		[pctl_devreset_lpd_timestamp] = crl_apb_rst_lpd_iou2 | (20 << 12),
		[pctl_devreset_lpd_rpu_r50] = crl_apb_rst_lpd_top | (0 << 12),
		[pctl_devreset_lpd_rpu_r51] = crl_apb_rst_lpd_top | (1 << 12),
		[pctl_devreset_lpd_rpu_amba] = crl_apb_rst_lpd_top | (2 << 12),
		[pctl_devreset_lpd_ocm] = crl_apb_rst_lpd_top | (3 << 12),
		[pctl_devreset_lpd_rpu_pge] = crl_apb_rst_lpd_top | (4 << 12),
		[pctl_devreset_lpd_usb0_corereset] = crl_apb_rst_lpd_top | (6 << 12),
		[pctl_devreset_lpd_usb1_corereset] = crl_apb_rst_lpd_top | (7 << 12),
		[pctl_devreset_lpd_usb0_hiberreset] = crl_apb_rst_lpd_top | (8 << 12),
		[pctl_devreset_lpd_usb1_hiberreset] = crl_apb_rst_lpd_top | (9 << 12),
		[pctl_devreset_lpd_usb0_apb] = crl_apb_rst_lpd_top | (10 << 12),
		[pctl_devreset_lpd_usb1_apb] = crl_apb_rst_lpd_top | (11 << 12),
		[pctl_devreset_lpd_ipi] = crl_apb_rst_lpd_top | (14 << 12),
		[pctl_devreset_lpd_apm] = crl_apb_rst_lpd_top | (15 << 12),
		[pctl_devreset_lpd_rtc] = crl_apb_rst_lpd_top | (16 << 12),
		[pctl_devreset_lpd_sysmon] = crl_apb_rst_lpd_top | (17 << 12),
		[pctl_devreset_lpd_s_axi_lpd] = crl_apb_rst_lpd_top | (19 << 12),
		[pctl_devreset_lpd_lpd_swdt] = crl_apb_rst_lpd_top | (20 << 12),
		[pctl_devreset_lpd_fpd] = crl_apb_rst_lpd_top | (23 << 12),
		[pctl_devreset_lpd_dbg_fpd] = crl_apb_rst_lpd_dbg | (0 << 12),
		[pctl_devreset_lpd_dbg_lpd] = crl_apb_rst_lpd_dbg | (1 << 12),
		[pctl_devreset_lpd_rpu_dbg0] = crl_apb_rst_lpd_dbg | (4 << 12),
		[pctl_devreset_lpd_rpu_dbg1] = crl_apb_rst_lpd_dbg | (5 << 12),
		[pctl_devreset_lpd_dbg_ack] = crl_apb_rst_lpd_dbg | (15 << 12),
		[pctl_devreset_fpd_sata] = crf_apb_rst_fpd_top | (1 << 12),
		[pctl_devreset_fpd_gt] = crf_apb_rst_fpd_top | (2 << 12),
		[pctl_devreset_fpd_gpu] = crf_apb_rst_fpd_top | (3 << 12),
		[pctl_devreset_fpd_gpu_pp0] = crf_apb_rst_fpd_top | (4 << 12),
		[pctl_devreset_fpd_gpu_pp1] = crf_apb_rst_fpd_top | (5 << 12),
		[pctl_devreset_fpd_fpd_dma] = crf_apb_rst_fpd_top | (6 << 12),
		[pctl_devreset_fpd_s_axi_hpc_0_fpd] = crf_apb_rst_fpd_top | (7 << 12),
		[pctl_devreset_fpd_s_axi_hpc_1_fpd] = crf_apb_rst_fpd_top | (8 << 12),
		[pctl_devreset_fpd_s_axi_hp_0_fpd] = crf_apb_rst_fpd_top | (9 << 12),
		[pctl_devreset_fpd_s_axi_hp_1_fpd] = crf_apb_rst_fpd_top | (10 << 12),
		[pctl_devreset_fpd_s_axi_hpc_2_fpd] = crf_apb_rst_fpd_top | (11 << 12),
		[pctl_devreset_fpd_s_axi_hpc_3_fpd] = crf_apb_rst_fpd_top | (12 << 12),
		[pctl_devreset_fpd_swdt] = crf_apb_rst_fpd_top | (15 << 12),
		[pctl_devreset_fpd_dp] = crf_apb_rst_fpd_top | (16 << 12),
		[pctl_devreset_fpd_pcie_ctrl] = crf_apb_rst_fpd_top | (17 << 12),
		[pctl_devreset_fpd_pcie_bridge] = crf_apb_rst_fpd_top | (18 << 12),
		[pctl_devreset_fpd_pcie_cfg] = crf_apb_rst_fpd_top | (19 << 12),
		[pctl_devreset_fpd_acpu0] = crf_apb_rst_fpd_apu | (0 << 12),
		[pctl_devreset_fpd_acpu1] = crf_apb_rst_fpd_apu | (1 << 12),
		[pctl_devreset_fpd_acpu2] = crf_apb_rst_fpd_apu | (2 << 12),
		[pctl_devreset_fpd_acpu3] = crf_apb_rst_fpd_apu | (3 << 12),
		[pctl_devreset_fpd_apu_l2] = crf_apb_rst_fpd_apu | (8 << 12),
		[pctl_devreset_fpd_acpu0_pwron] = crf_apb_rst_fpd_apu | (10 << 12),
		[pctl_devreset_fpd_acpu1_pwron] = crf_apb_rst_fpd_apu | (11 << 12),
		[pctl_devreset_fpd_acpu2_pwron] = crf_apb_rst_fpd_apu | (12 << 12),
		[pctl_devreset_fpd_acpu3_pwron] = crf_apb_rst_fpd_apu | (13 << 12),
		[pctl_devreset_fpd_ddr_apm] = crf_apb_rst_ddr_ss | (2 << 12),
		[pctl_devreset_fpd_ddr_reserved] = crf_apb_rst_ddr_ss | (3 << 12),
	};

	if ((dev < pctl_devreset_lpd_gem0) || (dev > pctl_devreset_fpd_ddr_reserved)) {
		return -1;
	}

	if (dev >= pctl_devreset_fpd_sata) {
		*reg = zynq_common.crf_apb + (lookup[dev] & ((1 << 12) - 1));
	}
	else {
		*reg = zynq_common.crl_apb + (lookup[dev] & ((1 << 12) - 1));
	}

	*bit = (1U << (lookup[dev] >> 12));
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


__attribute__((noreturn)) static void zynqmp_softRst(void)
{
	/* Equivalent to PS_SRST_B signal */
	*(zynq_common.crl_apb + crl_apb_reset_ctrl) |= (1 << 4);
	__builtin_unreachable();
}


volatile u32 *_zynq_ttc_getAddress(void)
{
	return _pmap_halMapDevice(TTC0_BASE_ADDR, 0, SIZE_PAGE);
}


void _zynq_ttc_performReset(void)
{
	(void)_zynq_setDevRst(pctl_devreset_lpd_ttc0, 0);
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
			else if (data->action == pctl_get)
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


static u32 checkNumCPUs(void)
{
	int i;
	u32 powerStatus, cpusAvailable;
	/* First check if MPIDR indicates uniprocessor system or no MP extensions */
	u64 mpidr = sysreg_read(mpidr_el1);
	if (((mpidr >> 30) & 0x2) != 0x2) {
		return 1;
	}

	powerStatus = (~*(zynq_common.crf_apb + crf_apb_rst_fpd_apu)) & 0xf;
	cpusAvailable = 0;
	for (i = 0; i < 4; i++, powerStatus >>= 1) {
		if ((powerStatus & 0x1) == 1) {
			cpusAvailable++;
		}
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
	zynq_common.nCpus = checkNumCPUs();
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
