/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ZYNQ-7000 basic peripherals control functions
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_ZYNQ7000_H_
#define _PHOENIX_ARCH_ZYNQ7000_H_


/* AMBA peripherals */
enum {
	pctl_amba_dma_clk = 0, pctl_amba_usb0_clk = 2, pctl_amba_usb1_clk, pctl_amba_gem0_clk = 6, pctl_amba_gem1_clk, pctl_amba_sdi0_clk = 10, pctl_amba_sdi1_clk,
	pctl_amba_spi0_clk = 14, pctl_amba_spi1_clk, pctl_amba_can0_clk, pctl_amba_can1_clk, pctl_amba_i2c0_clk, pctl_amba_i2c1_clk, pctl_amba_uart0_clk,
	pctl_amba_uart1_clk, pctl_amba_gpio_clk, pctl_amba_lqspi_clk, pctl_amba_smc_clk
};


/* Devices' clocks controllers */
enum {
	pctl_ctrl_usb0_clk = 0, pctl_ctrl_usb1_clk, pctl_ctrl_gem0_rclk, pctl_ctrl_gem1_rclk, pctl_ctrl_gem0_clk, pctl_ctrl_gem1_clk, pctl_ctrl_smc_clk,
	pctl_ctrl_lqspi_clk, pctl_ctrl_sdio_clk, pctl_ctrl_uart_clk, pctl_ctrl_spi_clk, pctl_ctrl_can_clk, pctl_ctrl_can_mioclk,
};


/* Devices' reset controllers */
enum {
	pctl_ctrl_pss_rst = 0, pctl_ctrl_ddr_rst, pctl_ctrl_topsw_rst, pctl_ctrl_dmac_rst, pctl_ctrl_usb_rst, pctl_ctrl_gem_rst, pctl_ctrl_sdio_rst,
	pctl_ctrl_spi_rst, pctl_ctrl_can_rst, pctl_ctrl_i2c_rst, pctl_ctrl_uart_rst, pctl_ctrl_gpio_rst, pctl_ctrl_lqspi_rst, pctl_ctrl_smc_rst, pctl_ctrl_ocm_rst,
	pctl_ctrl_fpga_rst, pctl_ctrl_a9_cpu_rst,
};


enum {
	pctl_mio_pin_00 = 0, pctl_mio_pin_01, pctl_mio_pin_02, pctl_mio_pin_03, pctl_mio_pin_04, pctl_mio_pin_05, pctl_mio_pin_06, pctl_mio_pin_07, pctl_mio_pin_08,
	pctl_mio_pin_09, pctl_mio_pin_10, pctl_mio_pin_11, pctl_mio_pin_12, pctl_mio_pin_13, pctl_mio_pin_14, pctl_mio_pin_15, pctl_mio_pin_16, pctl_pctl_mio_pin_17,
	pctl_mio_pin_18, pctl_mio_pin_19, pctl_mio_pin_20, pctl_mio_pin_21, pctl_mio_pin_22, pctl_mio_pin_23, pctl_mio_pin_24, pctl_mio_pin_25, pctl_mio_pin_26,
	pctl_mio_pin_27, pctl_mio_pin_28, pctl_mio_pin_29, pctl_mio_pin_30, pctl_mio_pin_31, pctl_mio_pin_32, pctl_mio_pin_33, pctl_mio_pin_34, pctl_mio_pin_35,
	pctl_mio_pin_36, pctl_mio_pin_37, pctl_mio_pin_38, pctl_mio_pin_39, pctl_mio_pin_40, pctl_mio_pin_41, pctl_mio_pin_42, pctl_mio_pin_43, pctl_mio_pin_44,
	pctl_mio_pin_45, pctl_mio_pin_46, pctl_mio_pin_47, pctl_mio_pin_48, pctl_mio_pin_49, pctl_mio_pin_50, pctl_mio_pin_51, pctl_mio_pin_52, pctl_mio_pin_53,
};


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_ambaclock = 0, pctl_devclock, pctl_mioclock, pctl_mio, pctl_devreset, pctl_reboot } type;

	union {
		struct {
			int dev;
			unsigned int state;
		} ambaclock;

		struct {
			int dev;
			char divisor0;
			char divisor1;
			char srcsel;
			char clkact0;
			char clkact1;
		} devclock;

		struct {
			int mio;
			char ref0;
			char mux0;
			char ref1;
			char mux1;
		} mioclock;

		struct {
			int pin;
			char disableRcvr;
			char pullup;
			char ioType;
			char speed;
			char l0;
			char l1;
			char l2;
			char l3;
			char triEnable;
		} mio;

		struct {
			int dev;
			unsigned int state;
		} devreset;

		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;
	};
} __attribute__((packed)) platformctl_t;

#endif
