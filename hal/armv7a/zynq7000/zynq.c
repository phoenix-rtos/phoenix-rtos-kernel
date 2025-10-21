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
#include "hal/hal.h"
#include "hal/armv7a/armv7a.h"
#include "hal/spinlock.h"
#include "include/arch/armv7a/zynq7000/zynq7000.h"
#include "hal/armv7a/zynq7000/zynq.h"


/* clang-format off */
/* SLCR (System Level Control Registers) */
enum {
	/* SLCR protection registers */
	slcr_scl = 0, slcr_lock, slcr_unlock, slcr_locksta,
	/* PLL configuration registers */
	slcr_arm_pll_ctrl = 0x40 ,slcr_ddr_pll_ctrl, slcr_io_pll_ctrl, slcr_pll_status, slcr_arm_pll_cfg, slcr_ddr_pll_cfg, slcr_io_pll_cfg,
	/* Clock control registers */
	slcr_arm_clk_ctrl = 0x48, slcr_ddr_clk_ctrl, slcr_dci_clk_ctrl, slcr_aper_clk_ctrl, slcr_usb0_clk_ctrl, slcr_usb1_clk_ctrl, slcr_gem0_rclk_ctrl,
	slcr_gem1_rclk_ctrl, slcr_gem0_clk_ctrl, slcr_gem1_clk_ctrl, slcr_smc_clk_ctrl, slcr_lqspi_clk_ctrl, slcr_sdio_clk_ctrl, slcr_uart_clk_ctrl,
	slcr_spi_clk_ctrl, slcr_can_clk_ctrl, slcr_can_mioclk_ctrl, slcr_dbg_clk_ctrl, slcr_pcap_clk_ctrl, slcr_topsw_clk_ctrl, slcr_fpga0_clk_ctrl,
	/* FPGA configuration registers */
	slcr_fpga0_thr_ctrl, slcr_fpga0_thr_cnt, slcr_fpga0_thr_sta, slcr_fpga1_clk_ctrl, slcr_fpga1_thr_ctrl, slcr_fpga1_thr_cnt, slcr_fpga1_thr_sta,
	slcr_fpga2_clk_ctrl, slcr_fpga2_thr_ctrl, slcr_fpga2_thr_cnt, slcr_fpga2_thr_sta, slcr_fpga3_clk_ctrl, slcr_fpga3_thr_ctrl, slcr_fpga3_thr_cnt,
	slcr_fpga3_thr_sta,
	/* Clock ratio register */
	slcr_clk_621_true = 0x71,
	/* Reset registers */
	slcr_pss_rst_ctrl = 0x80, slcr_ddr_rst_ctrl, slcr_topsw_rst_ctrl, slcr_dmac_rst_ctrl, slcr_usb_rst_ctrl, slcr_gem_rst_ctrl, slcr_sdio_rst_ctrl,
	slcr_spi_rst_ctrl, slcr_can_rst_ctrl, slcr_i2c_rst_ctrl, slcr_uart_rst_ctrl, slcr_gpio_rst_ctrl, slcr_lqspi_rst_ctrl, slcr_smc_rst_ctrl, slcr_ocm_rst_ctrl,
	slcr_fpga_rst_ctrl = 0x90, slcr_a9_cpu_rst_ctrl,
	/* APU watchdog register */
	slcr_rs_awdt_rst_ctrl = 0x93,
	slcr_reboot_status = 0x96, slcr_boot_mode,
	slcr_apu_control = 0xc0, slcr_wdt_clk_sel,
	slcr_tz_dma_ns = 0x110, slcr_tz_dma_irq_ns, slcr_tz_dma_periph_ns,
	slcr_pss_idcode = 0x14c,
	slcr_ddr_urgent = 0x180,
	slcr_ddr_cal_start = 0x183,
	slcr_ddr_ref_start = 0x185, slcr_ddr_cmd_sta, slcr_ddr_urgent_sel, slcr_ddr_dfi_status,
	/* MIO pins config registers */
	slcr_mio_pin_00 = 0x1c0, slcr_mio_pin_01, slcr_mio_pin_02, slcr_mio_pin_03, slcr_mio_pin_04, slcr_mio_pin_05, slcr_mio_pin_06, slcr_mio_pin_07, slcr_mio_pin_08,
	slcr_mio_pin_09, slcr_mio_pin_10, slcr_mio_pin_11, slcr_mio_pin_12, slcr_mio_pin_13, slcr_mio_pin_14, slcr_mio_pin_15, slcr_mio_pin_16, slcr_mio_pin_17,
	slcr_mio_pin_18, slcr_mio_pin_19, slcr_mio_pin_20, slcr_mio_pin_21, slcr_mio_pin_22, slcr_mio_pin_23, slcr_mio_pin_24, slcr_mio_pin_25, slcr_mio_pin_26,
	slcr_mio_pin_27, slcr_mio_pin_28, slcr_mio_pin_29, slcr_mio_pin_30, slcr_mio_pin_31, slcr_mio_pin_32, slcr_mio_pin_33, slcr_mio_pin_34, slcr_mio_pin_35,
	slcr_mio_pin_36, slcr_mio_pin_37, slcr_mio_pin_38, slcr_mio_pin_39, slcr_mio_pin_40, slcr_mio_pin_41, slcr_mio_pin_42, slcr_mio_pin_43, slcr_mio_pin_44,
	slcr_mio_pin_45, slcr_mio_pin_46, slcr_mio_pin_47, slcr_mio_pin_48, slcr_mio_pin_49, slcr_mio_pin_50, slcr_mio_pin_51, slcr_mio_pin_52, slcr_mio_pin_53,
	slcr_mio_loopback = 0x201,
	slcr_mio_mst_tri0 = 0x203, slcr_mio_mst_tri1,
	slcr_sd0_wp_cd_sel = 0x20c, slcr_sd1_wp_cd_sel,
	slcr_lvl_shftr_en = 0x240,
	slcr_ocm_cfg = 0x244,
	slcr_l2c_ram_reg = 0x287,
	/* GPIO config registers */
	slcr_gpiob_ctrl = 0x2c0, slcr_gpiob_cfg_cmos18, slcr_gpiob_cfg_cmos25, slcr_gpiob_cfg_cmos33,
	slcr_gpiob_cfg_hstl = 0x2c5, slcr_gpiob_drvr_bias_ctrl,
	/* DDR config registers */
	slcr_ddriob_addr0 = 0x2d0, slcr_ddriob_addr1, slcr_ddriob_data0, slcr_ddriob_data1, slcr_ddriob_diff0, slcr_ddriob_diff1, slcr_ddriob_clock, slcr_ddriob_drive_slew_addr,
	slcr_ddriob_drive_slew_data, slcr_ddriob_drive_slew_diff, slcr_ddriob_drive_slew_clock, slcr_ddriob_ddr_ctrl, slcr_ddriob_dci_ctrl, slcr_ddriob_dci_status,
};


enum {
	l2cc_ctrl = 0x40, l2cc_aux_ctrl, l2cc_tag_ram_ctrl, l2cc_data_ram_ctrl,
	l2cc_int_mask = 0x85, l2cc_int_mask_status, l2cc_int_raw, l2cc_int_clear,
	l2cc_sync = 0x1cc,
	l2cc_inval_pa = 0x1dc, l2cc_inval_way = 0x1df,
	l2cc_clean_pa = 0x1ec, l2cc_clean_index = 0x1ee, l2cc_clean_way,
	l2cc_flush_pa = 0x1fc, l2cc_flush_index = 0x1fe, l2cc_flush_way,
};
/* clang-format on */


static struct {
	spinlock_t pltctlSp;
	volatile u32 *slcr;
	volatile u32 *l2cc;
	unsigned int nCpus;
} zynq_common;


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _end;
/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Definition in assembly" */
volatile unsigned int nCpusStarted = 0;


static void _zynq_slcrLock(void)
{
	hal_cpuDataMemoryBarrier(); /* Ensure previous writes are committed before locking */
	*(zynq_common.slcr + slcr_lock) = 0x0000767b;
}


static void _zynq_slcrUnlock(void)
{
	*(zynq_common.slcr + slcr_unlock) = 0x0000df0d;
	hal_cpuDataMemoryBarrier(); /* Ensure subsequent writes are committed after unlocking */
}


int _zynq_setAmbaClk(u32 dev, u32 state)
{
	/* Check max dev position in amba register */
	if (dev > 24U) {
		return -1;
	}

	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_aper_clk_ctrl) = (*(zynq_common.slcr + slcr_aper_clk_ctrl) & ~(1U << dev)) | ((state == 0U ? 0UL : 1UL) << dev);
	_zynq_slcrLock();

	return 0;
}


static int _zynq_getAmbaClk(u32 dev, u32 *state)
{
	/* Check max dev position in amba register */
	if (dev > 24U) {
		return -1;
	}

	_zynq_slcrUnlock();
	*state = (*(zynq_common.slcr + slcr_aper_clk_ctrl) >> dev) & 0x1U;
	_zynq_slcrLock();

	return 0;
}


static int _zynq_setDevClk(u32 dev, u8 divisor0, u8 divisor1, u8 srcsel, u8 clkact0, u8 clkact1)
{
	u32 id = 0;
	int err = 0;

	_zynq_slcrUnlock();
	switch (dev) {
		case pctl_ctrl_usb0_clk:
		case pctl_ctrl_usb1_clk:
			id = dev - (u32)pctl_ctrl_usb0_clk;
			*(zynq_common.slcr + slcr_usb0_clk_ctrl + id) = (*(zynq_common.slcr + pctl_ctrl_usb0_clk + id) & ~0x00000070U) | ((u32)srcsel & 0x7U) << 4;
			break;

		case pctl_ctrl_gem0_rclk:
		case pctl_ctrl_gem1_rclk:
			id = dev - (u32)pctl_ctrl_gem0_rclk;
			*(zynq_common.slcr + slcr_gem0_rclk_ctrl + id) = (*(zynq_common.slcr + pctl_ctrl_gem0_rclk + id) & ~0x00000011U) | (clkact0 == 0U ? 0UL : 1UL) |
					((srcsel == 0U ? 0UL : 1UL) << 4);
			break;

		case pctl_ctrl_gem0_clk:
		case pctl_ctrl_gem1_clk:
			id = dev - (u32)pctl_ctrl_gem0_clk;
			*(zynq_common.slcr + slcr_gem0_clk_ctrl + id) = (*(zynq_common.slcr + slcr_gem0_clk_ctrl + id) & ~0x03f03f71U) | (clkact0 == 0U ? 0UL : 1UL) |
					(((u32)srcsel & 0x7U) << 4) | (((u32)divisor0 & 0x3fU) << 8) | (((u32)divisor1 & 0x3fU) << 20);
			break;

		case pctl_ctrl_smc_clk:
			*(zynq_common.slcr + slcr_smc_clk_ctrl) = (*(zynq_common.slcr + slcr_smc_clk_ctrl) & ~0x00003f31U) | (clkact0 == 0U ? 0UL : 1UL) |
					(((u32)srcsel & 0x3U) << 4) | (((u32)divisor0 & 0x3fU) << 8);
			break;

		case pctl_ctrl_lqspi_clk:
			*(zynq_common.slcr + slcr_lqspi_clk_ctrl) = (*(zynq_common.slcr + slcr_lqspi_clk_ctrl) & ~0x00003f31U) | (clkact0 == 0U ? 0UL : 1UL) |
					(((u32)srcsel & 0x3U) << 4) | (((u32)divisor0 & 0x3fU) << 8);
			break;

		case pctl_ctrl_sdio_clk:
			*(zynq_common.slcr + slcr_sdio_clk_ctrl) = (*(zynq_common.slcr + slcr_sdio_clk_ctrl) & ~0x00003f33U) | (clkact0 == 0U ? 0UL : 1UL) | ((clkact1 == 0U ? 0UL : 1UL) << 1) |
					(((u32)srcsel & 0x3U) << 4) | (((u32)divisor0 & 0x3fU) << 8);
			break;

		case pctl_ctrl_uart_clk:
			*(zynq_common.slcr + slcr_uart_clk_ctrl) = (*(zynq_common.slcr + slcr_uart_clk_ctrl) & ~0x00003f33U) | (clkact0 == 0U ? 0UL : 1UL) |
					((clkact1 == 0U ? 0UL : 1UL) << 1) | (((u32)srcsel & 0x3U) << 4) | (((u32)divisor0 & 0x3fU) << 8);
			break;

		case pctl_ctrl_spi_clk:
			*(zynq_common.slcr + slcr_spi_clk_ctrl) = (*(zynq_common.slcr + slcr_spi_clk_ctrl) & ~0x00003f33U) | (clkact0 == 0U ? 0UL : 1UL) |
					((clkact1 == 0U ? 0UL : 1UL) << 1) | (((u32)srcsel & 0x3U) << 4) | (((u32)divisor0 & 0x3fU) << 8);
			break;

		case pctl_ctrl_can_clk:
			*(zynq_common.slcr + slcr_can_clk_ctrl) = (*(zynq_common.slcr + slcr_can_clk_ctrl) & ~0x03f03f33U) | (clkact0 == 0U ? 0UL : 1UL) |
					((clkact1 == 0U ? 0UL : 1UL) << 1) | (((u32)srcsel & 0x3U) << 4) | (((u32)divisor0 & 0x3fU) << 8) | (((u32)divisor1 & 0x3fU) << 20);
			break;

		default:
			err = -1;
			break;
	}

	_zynq_slcrLock();

	return err;
}


static int _zynq_getDevClk(u32 dev, u8 *divisor0, u8 *divisor1, u8 *srcsel, u8 *clkact0, u8 *clkact1)
{
	u32 id;
	u32 val = 0;
	int err = 0;

	switch (dev) {
		case pctl_ctrl_usb0_clk:
		case pctl_ctrl_usb1_clk:
			id = dev - (u32)pctl_ctrl_usb0_clk;
			val = *(zynq_common.slcr + slcr_usb0_clk_ctrl + id);
			*srcsel = (u8)((val >> 4) & 0x7U);
			*clkact0 = *clkact1 = *divisor0 = *divisor1 = 0;
			break;

		case pctl_ctrl_gem0_rclk:
		case pctl_ctrl_gem1_rclk:
			id = dev - (u32)pctl_ctrl_gem0_rclk;
			val = *(zynq_common.slcr + slcr_gem0_rclk_ctrl + id);
			*clkact0 = (u8)(val & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x1U);
			*clkact1 = *divisor0 = *divisor1 = 0;
			break;

		case pctl_ctrl_gem0_clk:
		case pctl_ctrl_gem1_clk:
			id = dev - (u32)pctl_ctrl_gem0_clk;
			val = *(zynq_common.slcr + slcr_gem0_clk_ctrl + id);
			*clkact0 = (u8)(val & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x7U);
			*divisor0 = (u8)((val >> 8) & 0x3fU);
			*divisor1 = (u8)((val >> 20) & 0x3fU);
			*clkact1 = 0;
			break;

		case pctl_ctrl_smc_clk:
			val = *(zynq_common.slcr + slcr_smc_clk_ctrl);
			*clkact0 = (u8)(val & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x3U);
			*divisor0 = (u8)((val >> 8) & 0x3fU);
			*clkact1 = *divisor1 = 0;
			break;

		case pctl_ctrl_lqspi_clk:
			val = *(zynq_common.slcr + slcr_lqspi_clk_ctrl);
			*clkact0 = (u8)(val & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x3U);
			*divisor0 = (u8)((val >> 8) & 0x3fU);
			*clkact1 = *divisor1 = 0;
			break;

		case pctl_ctrl_sdio_clk:
			val = *(zynq_common.slcr + slcr_sdio_clk_ctrl);
			*clkact0 = (u8)(val & 0x1U);
			*clkact1 = (u8)((val >> 1) & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x3U);
			*divisor0 = (u8)((val >> 8) & 0x3fU);
			*divisor1 = 0;
			break;

		case pctl_ctrl_uart_clk:
			val = *(zynq_common.slcr + slcr_uart_clk_ctrl);
			*clkact0 = (u8)(val & 0x1U);
			*clkact1 = (u8)((val >> 1) & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x3U);
			*divisor0 = (u8)((val >> 8) & 0x3fU);
			*divisor1 = 0;
			break;

		case pctl_ctrl_spi_clk:
			val = *(zynq_common.slcr + slcr_spi_clk_ctrl);
			*clkact0 = (u8)(val & 0x1U);
			*clkact1 = (u8)((val >> 1) & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x3U);
			*divisor0 = (u8)((val >> 8) & 0x3fU);
			*divisor1 = 0;
			break;

		case pctl_ctrl_can_clk:
			val = *(zynq_common.slcr + slcr_can_clk_ctrl);
			*clkact0 = (u8)(val & 0x1U);
			*clkact1 = (u8)((val >> 1) & 0x1U);
			*srcsel = (u8)((val >> 4) & 0x3U);
			*divisor0 = (u8)((val >> 8) & 0x3fU);
			*divisor1 = (u8)((val >> 20) & 0x3fU);
			break;

		default:
			err = -1;
			break;
	}

	return err;
}


static int _zynq_setMioClk(u8 ref0, u8 mux0, u8 ref1, u8 mux1)
{
	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_can_mioclk_ctrl) = (*(zynq_common.slcr + slcr_can_mioclk_ctrl) & ~0x007f007fU) | ((u32)mux0 & 0x3fU) | ((ref0 == 0U ? 0UL : 1UL) << 6) |
			(((u32)mux1 & 0x3fU) << 16) | ((ref1 == 0U ? 0UL : 1UL) << 22);
	_zynq_slcrLock();

	return 0;
}


static int _zynq_getMioClk(u8 *ref0, u8 *mux0, u8 *ref1, u8 *mux1)
{
	u32 val = 0;

	val = *(zynq_common.slcr + slcr_can_mioclk_ctrl);
	*mux0 = (u8)(val & 0x3fU);
	*ref0 = (u8)((val >> 6) & 0x1U);
	*mux1 = (u8)((val >> 16) & 0x3fU);
	*ref1 = (u8)((val >> 22) & 0x1U);

	return 0;
}


int _zynq_setMIO(unsigned int pin, u8 disableRcvr, u8 pullup, u8 ioType, u8 speed, u8 l0, u8 l1, u8 l2, u8 l3, u8 triEnable)
{
	u32 val = 0;

	if (pin > 53U) {
		return -1;
	}

	val = ((triEnable == 0U ? 0UL : 1UL)) | ((l0 == 0U ? 0UL : 1UL) << 1) | ((l1 == 0U ? 0UL : 1UL) << 2) | ((l2 & 0x3UL) << 3) |
			((l3 & 0x7UL) << 5) | ((speed == 0U ? 0UL : 1UL) << 8) | ((ioType & 0x7UL) << 9) | ((pullup == 0U ? 0UL : 1UL) << 12) |
			((disableRcvr == 0U ? 0UL : 1UL) << 13);

	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_mio_pin_00 + pin) = (*(zynq_common.slcr + slcr_mio_pin_00 + pin) & ~0x00003fffU) | val;
	_zynq_slcrLock();

	return 0;
}


static int _zynq_getMIO(unsigned int pin, u8 *disableRcvr, u8 *pullup, u8 *ioType, u8 *speed, u8 *l0, u8 *l1, u8 *l2, u8 *l3, u8 *triEnable)
{
	u32 val;

	if (pin > 53U) {
		return -1;
	}

	val = *(zynq_common.slcr + slcr_mio_pin_00 + pin);

	*disableRcvr = (u8)((val >> 13) & 0x1U);
	*pullup = (u8)((val >> 12) & 0x1U);
	*ioType = (u8)((val >> 9) & 0x7U);
	*speed = (u8)((val >> 8) & 0x1U);
	*l0 = (u8)((val >> 1) & 0x1U);
	*l1 = (u8)((val >> 2) & 0x1U);
	*l2 = (u8)((val >> 3) & 0x3U);
	*l3 = (u8)((val >> 5) & 0x7U);
	*triEnable = (u8)(val & 0x1U);

	return 0;
}


static int _zynq_setDevRst(u32 dev, unsigned int state)
{
	int err = 0;

	_zynq_slcrUnlock();
	switch (dev) {
		case pctl_ctrl_pss_rst:
			*(zynq_common.slcr + slcr_pss_rst_ctrl) = state;
			break;

		case pctl_ctrl_ddr_rst:
			*(zynq_common.slcr + slcr_ddr_rst_ctrl) = state;
			break;

		case pctl_ctrl_topsw_rst:
			*(zynq_common.slcr + slcr_topsw_rst_ctrl) = state;
			break;

		case pctl_ctrl_dmac_rst:
			*(zynq_common.slcr + slcr_dmac_rst_ctrl) = state;
			break;

		case pctl_ctrl_usb_rst:
			*(zynq_common.slcr + slcr_usb_rst_ctrl) = state;
			break;

		case pctl_ctrl_gem_rst:
			*(zynq_common.slcr + slcr_gem_rst_ctrl) = state;
			break;

		case pctl_ctrl_sdio_rst:
			*(zynq_common.slcr + slcr_sdio_rst_ctrl) = state;
			break;

		case pctl_ctrl_spi_rst:
			*(zynq_common.slcr + slcr_spi_rst_ctrl) = state;
			break;

		case pctl_ctrl_can_rst:
			*(zynq_common.slcr + slcr_can_rst_ctrl) = state;
			break;

		case pctl_ctrl_i2c_rst:
			*(zynq_common.slcr + slcr_i2c_rst_ctrl) = state;
			break;

		case pctl_ctrl_uart_rst:
			*(zynq_common.slcr + slcr_uart_rst_ctrl) = state;
			break;

		case pctl_ctrl_gpio_rst:
			*(zynq_common.slcr + slcr_gpio_rst_ctrl) = state;
			break;

		case pctl_ctrl_lqspi_rst:
			*(zynq_common.slcr + slcr_lqspi_rst_ctrl) = state;
			break;

		case pctl_ctrl_smc_rst:
			*(zynq_common.slcr + slcr_smc_rst_ctrl) = state;
			break;

		case pctl_ctrl_ocm_rst:
			*(zynq_common.slcr + slcr_ocm_rst_ctrl) = state;
			break;

		case pctl_ctrl_fpga_rst:
			*(zynq_common.slcr + slcr_fpga_rst_ctrl) = state;
			break;

		case pctl_ctrl_a9_cpu_rst:
			*(zynq_common.slcr + slcr_a9_cpu_rst_ctrl) = state;
			break;

		default:
			err = -1;
			break;
	}
	_zynq_slcrLock();

	return err;
}


static int _zynq_getDevRst(u32 dev, unsigned int *state)
{
	int err = 0;

	switch (dev) {
		case pctl_ctrl_pss_rst:
			*state = *(zynq_common.slcr + slcr_pss_rst_ctrl);
			break;

		case pctl_ctrl_ddr_rst:
			*state = *(zynq_common.slcr + slcr_ddr_rst_ctrl);
			break;

		case pctl_ctrl_topsw_rst:
			*state = *(zynq_common.slcr + slcr_topsw_rst_ctrl);
			break;

		case pctl_ctrl_dmac_rst:
			*state = *(zynq_common.slcr + slcr_dmac_rst_ctrl);
			break;

		case pctl_ctrl_usb_rst:
			*state = *(zynq_common.slcr + slcr_usb_rst_ctrl);
			break;

		case pctl_ctrl_gem_rst:
			*state = *(zynq_common.slcr + slcr_gem_rst_ctrl);
			break;

		case pctl_ctrl_sdio_rst:
			*state = *(zynq_common.slcr + slcr_sdio_rst_ctrl);
			break;

		case pctl_ctrl_spi_rst:
			*state = *(zynq_common.slcr + slcr_spi_rst_ctrl);
			break;

		case pctl_ctrl_can_rst:
			*state = *(zynq_common.slcr + slcr_can_rst_ctrl);
			break;

		case pctl_ctrl_i2c_rst:
			*state = *(zynq_common.slcr + slcr_i2c_rst_ctrl);
			break;

		case pctl_ctrl_uart_rst:
			*state = *(zynq_common.slcr + slcr_uart_rst_ctrl);
			break;

		case pctl_ctrl_gpio_rst:
			*state = *(zynq_common.slcr + slcr_gpio_rst_ctrl);
			break;

		case pctl_ctrl_lqspi_rst:
			*state = *(zynq_common.slcr + slcr_lqspi_rst_ctrl);
			break;

		case pctl_ctrl_smc_rst:
			*state = *(zynq_common.slcr + slcr_smc_rst_ctrl);
			break;

		case pctl_ctrl_ocm_rst:
			*state = *(zynq_common.slcr + slcr_ocm_rst_ctrl);
			break;

		case pctl_ctrl_fpga_rst:
			*state = *(zynq_common.slcr + slcr_fpga_rst_ctrl);
			break;

		case pctl_ctrl_a9_cpu_rst:
			*state = *(zynq_common.slcr + slcr_a9_cpu_rst_ctrl);
			break;

		default:
			err = -1;
			break;
	}

	return err;
}


__attribute__((noreturn)) static void zynq_softRst(void)
{
	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_pss_rst_ctrl) |= 0x1U;
	_zynq_slcrLock();

	__builtin_unreachable();
}


static int _zynq_setSDWpCd(u32 dev, u8 wpPin, u8 cdPin)
{
	if ((dev != 0U) && (dev != 1U)) {
		return -1;
	}

	if ((cdPin > 63U) || (wpPin > 63U)) {
		return -1;
	}

	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_sd0_wp_cd_sel + dev) = ((u32)cdPin << 16) | (wpPin);
	_zynq_slcrLock();
	return 0;
}


static int _zynq_getSDWpCd(u32 dev, u8 *wpPin, u8 *cdPin)
{
	u32 val = 0;
	if ((dev != 0U) && (dev != 1U)) {
		return -1;
	}

	val = *(zynq_common.slcr + slcr_sd0_wp_cd_sel + dev);
	*wpPin = (u8)(val & 0x3fU);
	*cdPin = (u8)((val >> 16) & 0x3fU);
	return 0;
}


__attribute__((noreturn)) void hal_cpuReboot(void)
{
	zynq_softRst();
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
		case pctl_ambaclock:
			if (data->action == pctl_set) {
				ret = _zynq_setAmbaClk((u32)data->ambaclock.dev, data->ambaclock.state);
			}
			else if (data->action == pctl_get) {
				ret = _zynq_getAmbaClk((u32)data->ambaclock.dev, &t);
				data->ambaclock.state = t;
			}
			else {
				/* No action required */
			}
			break;

		case pctl_mioclock:
			if (data->mioclock.mio == pctl_ctrl_can_mioclk) {
				if (data->action == pctl_set) {
					ret = _zynq_setMioClk(data->mioclock.ref0, data->mioclock.mux0, data->mioclock.ref1, data->mioclock.mux1);
				}
				else if (data->action == pctl_get) {
					ret = _zynq_getMioClk(&data->mioclock.ref0, &data->mioclock.mux0, &data->mioclock.ref1, &data->mioclock.mux1);
				}
				else {
					/* No action required */
				}
			}
			break;

		case pctl_devclock:
			if (data->action == pctl_set) {
				ret = _zynq_setDevClk((u32)data->devclock.dev, data->devclock.divisor0, data->devclock.divisor1,
						data->devclock.srcsel, data->devclock.clkact0, data->devclock.clkact1);
			}
			else if (data->action == pctl_get) {
				ret = _zynq_getDevClk((u32)data->devclock.dev, &data->devclock.divisor0, &data->devclock.divisor1,
						&data->devclock.srcsel, &data->devclock.clkact0, &data->devclock.clkact1);
			}
			else {
				/* No actionrequired */
			}
			break;

		case pctl_mio:
			if (data->action == pctl_set) {
				ret = _zynq_setMIO((unsigned int)data->mio.pin, data->mio.disableRcvr, data->mio.pullup, data->mio.ioType, data->mio.speed, data->mio.l0,
						data->mio.l1, data->mio.l2, data->mio.l3, data->mio.triEnable);
			}
			else if (data->action == pctl_get) {
				ret = _zynq_getMIO((unsigned int)data->mio.pin, &data->mio.disableRcvr, &data->mio.pullup, &data->mio.ioType, &data->mio.speed, &data->mio.l0,
						&data->mio.l1, &data->mio.l2, &data->mio.l3, &data->mio.triEnable);
			}
			else {
				/* No action required */
			}
			break;

		case pctl_devreset:
			if (data->action == pctl_set) {
				ret = _zynq_setDevRst((u32)data->devreset.dev, data->devreset.state);
			}
			else if (data->action == pctl_get) {
				ret = _zynq_getDevRst((u32)data->devreset.dev, &t);
				data->devreset.state = t;
			}
			else {
				/* No action required */
			}
			break;

		case pctl_reboot:
			if ((data->action == pctl_set) && (data->reboot.magic == PCTL_REBOOT_MAGIC)) {
				zynq_softRst();
			}
			/* TODO add boot reason for pctl_get */
			break;

		/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
		case pctl_sdwpcd:
			if (data->action == pctl_set) {
				ret = _zynq_setSDWpCd((u32)data->SDWpCd.dev, data->SDWpCd.wpPin, data->SDWpCd.cdPin);
			}
			else { /* data->action == pctl_get */
				ret = _zynq_getSDWpCd((u32)data->SDWpCd.dev, &data->SDWpCd.wpPin, &data->SDWpCd.cdPin);
			}
			/* Fall-through*/

		default:
			/* No action required */
			break;
	}

	hal_spinlockClear(&zynq_common.pltctlSp, &sc);

	return ret;
}


static void _zynq_activateL2Cache(void)
{
	*(zynq_common.l2cc + l2cc_ctrl) = 0; /* Disable L2 cache */
	hal_cpuDataMemoryBarrier();
	*(zynq_common.l2cc + l2cc_aux_ctrl) |= 0x72360000U; /* Enable all prefetching, Way Size (16 KB) and High Priority for SO and Dev Reads Enable */
	*(zynq_common.l2cc + l2cc_tag_ram_ctrl) = 0x0111;   /* 7 Cycles of latency for TAG RAM */
	*(zynq_common.l2cc + l2cc_data_ram_ctrl) = 0x0121;  /* 7 Cycles of latency for DATA RAM */
	*(zynq_common.l2cc + l2cc_inval_way) = 0xFFFF;      /* Invalidate everything */
	hal_cpuDataMemoryBarrier();
	while (*(zynq_common.l2cc + l2cc_sync) != 0U) {
		/* wait for completion */
	}

	*(zynq_common.l2cc + l2cc_int_clear) = *(zynq_common.l2cc + l2cc_int_raw); /* Clear pending interrupts */
	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_l2c_ram_reg) = 0x00020202; /* Magic value, not described in detail */
	_zynq_slcrLock();
	hal_cpuDataMemoryBarrier();
	*(zynq_common.l2cc + l2cc_ctrl) |= 1U; /* Enable L2 cache */
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&zynq_common.pltctlSp, "pltctl");
	zynq_common.slcr = (void *)(((u32)&_end + 9U * SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U));
	zynq_common.l2cc = (void *)(((u32)&_end + 7U * SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U));
}


unsigned int hal_cpuGetCount(void)
{
	return zynq_common.nCpus;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static u32 checkNumCPUs(void)
{
	/* First check if MPIDR indicates uniprocessor system or no MP extensions */
	unsigned int mpidr;
	/* clang-format off */
	__asm__ volatile ("mrc p15, 0, %0, c0, c0, 5": "=r"(mpidr));
	/* clang-format on */
	if ((mpidr >> 30) != 0x2U) {
		return 1;
	}

	/* Otherwise we are in a multiprocessor system and we can check SCU for number of cores in SMP */
	volatile u32 *scu = (void *)(((u32)&_end + 5U * SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U));
	/* We cannot use SCU_CPU_Power_Status_Register because it's not implemented correctly on QEMU */
	u32 powerStatus = (*(scu + 1)) >> 4; /* SCU_CONFIGURATION_REGISTER */
	u32 cpusAvailable = 0;
	for (int i = 0; i < 4; i++) {
		if ((powerStatus & 0x1U) == 1U) {
			cpusAvailable++;
		}
		powerStatus >>= 1;
	}

	return cpusAvailable;
}


void _hal_cpuInit(void)
{
	zynq_common.nCpus = checkNumCPUs();
	hal_cpuAtomicInc(&nCpusStarted);
	if (hal_cpuAtomicGet(&nCpusStarted) == 1U) {
		/* This is necessary because other CPU is still in physical memory
		 * with L1 cache turned off so SCU cannot enforce cache coherence */
		hal_cpuFlushDataCache((ptr_t)&nCpusStarted, (ptr_t)((&nCpusStarted) + 1));
	}

	hal_cpuSignalEvent();
	while (hal_cpuAtomicGet(&nCpusStarted) != zynq_common.nCpus) {
		hal_cpuWaitForEvent();
	}

	if (hal_cpuGetID() == 0U) {
		_zynq_activateL2Cache();
	}
}


void hal_cpuSmpSync(void)
{
	/* TODO: not implemented yet */
}
