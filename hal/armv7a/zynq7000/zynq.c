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

#include "../../cpu.h"
#include "../../spinlock.h"
#include "../../include/arch/zynq7000.h"


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
	/* GPIO config registers */
	slcr_gpiob_ctrl = 0x2c0, slcr_gpiob_cfg_cmos18, slcr_gpiob_cfg_cmos25, slcr_gpiob_cfg_cmos33,
	slcr_gpiob_cfg_hstl = 0x2c5, slcr_gpiob_drvr_bias_ctrl,
	/* DDR config registers */
	slcr_ddriob_addr0 = 0x2d0, slcr_ddriob_addr1, slcr_ddriob_data0, slcr_ddriob_data1, slcr_ddriob_diff0, slcr_ddriob_diff1, slcr_ddriob_clock, slcr_ddriob_drive_slew_addr,
	slcr_ddriob_drive_slew_data, slcr_ddriob_drive_slew_diff, slcr_ddriob_drive_slew_clock, slcr_ddriob_ddr_ctrl, slcr_ddriob_dci_ctrl, slcr_ddriob_dci_status,
};


struct {
	spinlock_t pltctlSp;
	volatile u32 *slcr;
} zynq_common;


extern unsigned int _end;


static void _zynq_slcrLock(void)
{
	*(zynq_common.slcr + slcr_lock) = 0x0000767b;
}


static void _zynq_slcrUnlock(void)
{
	*(zynq_common.slcr + slcr_unlock) = 0x0000df0d;
}


int _zynq_setAmbaClk(u32 dev, u32 state)
{
	/* Check max dev position in amba register */
	if (dev > 24)
		return -1;

	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_aper_clk_ctrl) = (*(zynq_common.slcr + slcr_aper_clk_ctrl) & ~(1 << dev)) | (!!state << dev);
	_zynq_slcrLock();

	return 0;
}


static int _zynq_getAmbaClk(u32 dev, u32 *state)
{
	/* Check max dev position in amba register */
	if (dev > 24)
		return -1;

	_zynq_slcrUnlock();
	*state = (*(zynq_common.slcr + slcr_aper_clk_ctrl) >> dev) & 0x1;
	_zynq_slcrLock();

	return 0;
}


static int _zynq_setDevClk(int dev, char divisor0, char divisor1, char srcsel, char clkact0, char clkact1)
{
	u32 id = 0;
	int err = 0;

	_zynq_slcrUnlock();
	switch (dev) {
		case pctl_ctrl_usb0_clk:
		case pctl_ctrl_usb1_clk:
			id = dev - pctl_ctrl_usb0_clk;
			*(zynq_common.slcr + slcr_usb0_clk_ctrl + id) = (*(zynq_common.slcr + pctl_ctrl_usb0_clk + id) & ~0x00000070) | (srcsel & 0x7) << 4;
			break;

		case pctl_ctrl_gem0_rclk:
		case pctl_ctrl_gem1_rclk:
			id = dev - pctl_ctrl_gem0_rclk;
			*(zynq_common.slcr + slcr_gem0_rclk_ctrl + id) = (*(zynq_common.slcr + pctl_ctrl_gem0_rclk + id) & ~0x00000011) | (!!clkact0) |
				((!!srcsel) << 4);
			break;

		case pctl_ctrl_gem0_clk:
		case pctl_ctrl_gem1_clk:
			id = dev - pctl_ctrl_gem0_clk;
			*(zynq_common.slcr + slcr_gem0_clk_ctrl + id) = (*(zynq_common.slcr + slcr_gem0_clk_ctrl + id) & ~0x03f03f71) | (!!clkact0) |
				((srcsel & 0x7) << 4) | ((divisor0 & 0x3f) << 8) | ((divisor1 & 0x3f) << 20);
			break;

		case pctl_ctrl_smc_clk:
			*(zynq_common.slcr + slcr_smc_clk_ctrl) = (*(zynq_common.slcr + slcr_smc_clk_ctrl) & ~0x00003f31) | (!!clkact0) |
				((srcsel & 0x3) << 4) | ((divisor0 & 0x3f) << 8);
			break;

		case pctl_ctrl_lqspi_clk:
			*(zynq_common.slcr + slcr_lqspi_clk_ctrl) = (*(zynq_common.slcr + slcr_lqspi_clk_ctrl) & ~0x00003f31) | (!!clkact0) |
				((srcsel & 0x3) << 4) | ((divisor0 & 0x3f) << 8);
			break;

		case pctl_ctrl_sdio_clk:
			*(zynq_common.slcr + slcr_sdio_clk_ctrl) = (*(zynq_common.slcr + slcr_sdio_clk_ctrl) & ~0x00003f33) | (!!clkact0) | ((!!clkact1) << 1) |
				((srcsel & 0x3) << 4) | ((divisor0 & 0x3f) << 8);
			break;

		case pctl_ctrl_uart_clk:
			*(zynq_common.slcr + slcr_uart_clk_ctrl) = (*(zynq_common.slcr + slcr_uart_clk_ctrl) & ~0x00003f33) | (!!clkact0) |
				((!!clkact1) << 1) | ((srcsel & 0x3) << 4) | ((divisor0 & 0x3f) << 8);
			break;

		case pctl_ctrl_spi_clk:
			*(zynq_common.slcr + slcr_spi_clk_ctrl) = (*(zynq_common.slcr + slcr_spi_clk_ctrl) & ~0x00003f33) | (!!clkact0) |
				((!!clkact1) << 1) | ((srcsel & 0x3) << 4) | ((divisor0 & 0x3f) << 8);
			break;

		case pctl_ctrl_can_clk:
			*(zynq_common.slcr + slcr_can_clk_ctrl) = (*(zynq_common.slcr + slcr_can_clk_ctrl) & ~0x03f03f33) | (!!clkact0) |
				((!!clkact1) << 1) | ((srcsel & 0x3) << 4) | ((divisor0 & 0x3f) << 8) | ((divisor1 & 0x3f) << 20);
			break;

		default:
			err = -1;
			break;
	}

	_zynq_slcrLock();

	return err;
}


static int _zynq_getDevClk(int dev, char *divisor0, char *divisor1, char *srcsel, char *clkact0, char *clkact1)
{
	u32 id;
	u32 val = 0;
	int err = 0;

	switch (dev) {
		case pctl_ctrl_usb0_clk:
		case pctl_ctrl_usb1_clk:
			id = dev - pctl_ctrl_usb0_clk;
			val = *(zynq_common.slcr + slcr_usb0_clk_ctrl + id);
			*srcsel = (val >> 4) & 0x7;
			*clkact0 = *clkact1 = *divisor0 = *divisor1 = 0;
			break;

		case pctl_ctrl_gem0_rclk:
		case pctl_ctrl_gem1_rclk:
			id = dev - pctl_ctrl_gem0_rclk;
			val = *(zynq_common.slcr + slcr_gem0_rclk_ctrl + id);
			*clkact0 = val & 0x1;
			*srcsel = (val >> 4) & 0x1;
			*clkact1 = *divisor0 = *divisor1 = 0;
			break;

		case pctl_ctrl_gem0_clk:
		case pctl_ctrl_gem1_clk:
			id = dev - pctl_ctrl_gem0_clk;
			val = *(zynq_common.slcr + slcr_gem0_clk_ctrl + id);
			*clkact0 = val & 0x1;
			*srcsel = (val >> 4) & 0x7;
			*divisor0 = (val >> 8) & 0x3f;
			*divisor1 = (val >> 20) & 0x3f;
			*clkact1 = 0;
			break;

		case pctl_ctrl_smc_clk:
			val = *(zynq_common.slcr + slcr_smc_clk_ctrl);
			*clkact0 = val & 0x1;
			*srcsel = (val >> 4) & 0x3;
			*divisor0 = (val >> 8) & 0x3f;
			*clkact1 = *divisor1 = 0;
			break;

		case pctl_ctrl_lqspi_clk:
			val = *(zynq_common.slcr + slcr_lqspi_clk_ctrl);
			*clkact0 = val & 0x1;
			*srcsel = (val >> 4) & 0x3;
			*divisor0 = (val >> 8) & 0x3f;
			*clkact1 = *divisor1 = 0;
			break;

		case pctl_ctrl_sdio_clk:
			val = *(zynq_common.slcr + slcr_sdio_clk_ctrl);
			*clkact0 = val & 0x1;
			*clkact1 = (val >> 1) & 0x1;
			*srcsel = (val >> 4) & 0x3;
			*divisor0 = (val >> 8) & 0x3f;
			*divisor1 = 0;
			break;

		case pctl_ctrl_uart_clk:
			val = *(zynq_common.slcr + slcr_uart_clk_ctrl);
			*clkact0 = val & 0x1;
			*clkact1 = (val >> 1) & 0x1;
			*srcsel = (val >> 4) & 0x3;
			*divisor0 = (val >> 8) & 0x3f;
			*divisor1 = 0;
			break;

		case pctl_ctrl_spi_clk:
			val = *(zynq_common.slcr + slcr_spi_clk_ctrl);
			*clkact0 = val & 0x1;
			*clkact1 = (val >> 1) & 0x1;
			*srcsel = (val >> 4) & 0x3;
			*divisor0 = (val >> 8) & 0x3f;
			*divisor1 = 0;
			break;

		case pctl_ctrl_can_clk:
			val = *(zynq_common.slcr + slcr_can_clk_ctrl);
			*clkact0 = val & 0x1;
			*clkact1 = (val >> 1) & 0x1;
			*srcsel = (val >> 4) & 0x3;
			*divisor0 = (val >> 8) & 0x3f;
			*divisor1 = (val >> 20) & 0x3f;
			break;

		default:
			err = -1;
			break;
	}

	return err;
}


static int _zynq_setMioClk(char ref0, char mux0, char ref1, char mux1)
{
	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_can_mioclk_ctrl) = (*(zynq_common.slcr + slcr_can_mioclk_ctrl) & ~0x007f007f) | (mux0 & 0x3f) | ((!!ref0) << 6) |
		((mux1 & 0x3f) << 16) | ((!!ref1) << 22);
	_zynq_slcrLock();

	return 0;
}


static int _zynq_getMioClk(char *ref0, char *mux0, char *ref1, char *mux1)
{
	u32 val = 0;

	val = *(zynq_common.slcr + slcr_can_mioclk_ctrl);
	*mux0 = val & 0x3f;
	*ref0 = (val >> 6) & 0x1;
	*mux1 = (val >> 16) & 0x3f;
	*ref1 = (val >> 22) & 0x1;

	return 0;
}


int _zynq_setMIO(unsigned int pin, char disableRcvr, char pullup, char ioType, char speed, char l0, char l1, char l2, char l3, char triEnable)
{
	u32 val = 0;

	if (pin > 53)
		return -1;

	val = (!!triEnable) | (!!l0 << 1) | (!!l1 << 2) | ((l2 & 0x3) << 3) |
		((l3 & 0x7) << 5) | (!!speed << 8) | ((ioType & 0x7) << 9) | (!!pullup << 12) |
		(!!disableRcvr << 13);

	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_mio_pin_00 + pin) = (*(zynq_common.slcr + slcr_mio_pin_00 + pin) & ~0x00003fff) | val;
	_zynq_slcrLock();

	return 0;
}


static int _zynq_getMIO(unsigned int pin, char *disableRcvr, char *pullup, char *ioType, char *speed, char *l0, char *l1, char *l2, char *l3, char *triEnable)
{
	u32 val;

	if (pin > 53)
		return -1;

	val = *(zynq_common.slcr + slcr_mio_pin_00 + pin);

	*disableRcvr = (val >> 13) & 0x1;
	*pullup = (val >> 12) & 0x1;
	*ioType = (val >> 9) & 0x7;
	*speed = (val >> 8) & 0x1;
	*l0 = (val >> 1) & 0x1;
	*l1 = (val >> 2) & 0x1;
	*l2 = (val >> 3) & 0x3;
	*l3 = (val >> 5) & 0x7;
	*triEnable = val & 0x1;

	return 0;
}


static int _zynq_setDevRst(int dev, unsigned int state)
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


static int _zynq_getDevRst(int dev, unsigned int *state)
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


static void zynq_softRst(void)
{
	_zynq_slcrUnlock();
	*(zynq_common.slcr + slcr_pss_rst_ctrl) |= 0x1;
	_zynq_slcrLock();

	__builtin_unreachable();
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
				ret = _zynq_setAmbaClk(data->ambaclock.dev, data->ambaclock.state);
			}
			else if (data->action == pctl_get) {
				ret = _zynq_getAmbaClk(data->ambaclock.dev, &t);
				data->ambaclock.state = t;
			}
			break;

		case pctl_mioclock:
			if (data->mioclock.mio == pctl_ctrl_can_mioclk) {
				if (data->action == pctl_set)
					ret = _zynq_setMioClk(data->mioclock.ref0, data->mioclock.mux0, data->mioclock.ref1, data->mioclock.mux1);
				else if (data->action == pctl_get)
					ret = _zynq_getMioClk(&data->mioclock.ref0, &data->mioclock.mux0, &data->mioclock.ref1, &data->mioclock.mux1);
			}
			break;

		case pctl_devclock:
			if (data->action == pctl_set)
				ret = _zynq_setDevClk(data->devclock.dev, data->devclock.divisor0, data->devclock.divisor1,
					data->devclock.srcsel, data->devclock.clkact0, data->devclock.clkact1);
			else if (data->action == pctl_get)
				ret = _zynq_getDevClk(data->devclock.dev, &data->devclock.divisor0, &data->devclock.divisor1,
					&data->devclock.srcsel, &data->devclock.clkact0, &data->devclock.clkact1);
			break;

		case pctl_mio:
			if (data->action == pctl_set)
				ret = _zynq_setMIO(data->mio.pin, data->mio.disableRcvr, data->mio.pullup, data->mio.ioType, data->mio.speed, data->mio.l0,
					data->mio.l1, data->mio.l2, data->mio.l3, data->mio.triEnable);
			else if (data->action == pctl_get)
				ret = _zynq_getMIO(data->mio.pin, &data->mio.disableRcvr, &data->mio.pullup, &data->mio.ioType, &data->mio.speed, &data->mio.l0,
					&data->mio.l1, &data->mio.l2, &data->mio.l3, &data->mio.triEnable);
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
			zynq_softRst();
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
	zynq_common.slcr = (void *)(((u32)&_end + 9 * SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
}
