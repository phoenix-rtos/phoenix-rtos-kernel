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

#ifndef _PHOENIX_ARCH_IMX6ULL_H_
#define _PHOENIX_ARCH_IMX6ULL_H_

/* Devices clocks */
enum {
	/* CCM_CCGR0 */
	pctl_clk_aips_tz1 = 0, pctl_clk_aips_tz2, pctl_clk_apbhdma, pctl_clk_asrc, pctl_clk_dcp = pctl_clk_asrc + 2,
	pctl_clk_enet, pctl_clk_can1, pctl_clk_can1_serial, pctl_clk_can2, pctl_clk_can2_serial, pctl_clk_arm_dbg,
	pctl_clk_gpt2, pctl_clk_gpt2_serial, pctl_clk_uart2, pctl_clk_gpio2,

	/* CCM_CCGR1 */
	pctl_clk_ecspi1, pctl_clk_ecspi2, pctl_clk_ecspi3, pctl_clk_ecspi4, pctl_clk_adc2, pctl_clk_uart3,
	pctl_clk_epit1, pctl_clk_epit2, pctl_clk_adc1, pctl_clk_sim_s, pctl_clk_gpt, pctl_clk_gpt_serial,
	pctl_clk_uart4, pctl_clk_gpio1, pctl_clk_csu, pctl_clk_gpio5,

	/* CCM_CCGR2 */
	pctl_clk_esai = pctl_clk_gpio5 + 1, pctl_clk_csi, pctl_clk_iomuxc_snvs, pctl_clk_i2c1_serial,
	pctl_clk_i2c2_serial, pctl_clk_i2c3_serial, pctl_clk_ocotp_ctrl, pctl_clk_iomux_ipt_clk_io, pctl_clk_ipmux1,
	pctl_clk_ipmux2, pctl_clk_ipmux3, pctl_clk_ipsync_ip2apb_tzasc1_ipg,
	pctl_clk_gpio3 = pctl_clk_ipsync_ip2apb_tzasc1_ipg + 2, pctl_clk_lcd, pctl_clk_pxp,

	/* CCM_CCGR3 */
	pctl_clk_uart5 = pctl_clk_pxp + 2, pctl_clk_epdc, pctl_clk_uart6, pctl_clk_ca7_ccm_dap, pctl_clk_lcdif1_pix,
	pctl_clk_gpio4, pctl_clk_qspi, pctl_clk_wdog1, pctl_clk_a7_clkdiv_patch, pctl_clk_mmdc_core_aclk_fast_core_p0,
	pctl_clk_mmdc_core_ipg_clk_p0 = pctl_clk_mmdc_core_aclk_fast_core_p0 + 2, pctl_clk_mmdc_core_ipg_clk_p1,
	pctl_clk_ocram, pctl_clk_iomux_snvs_gpr,

	/* CCM_CCGR4 */
	pctl_clk_iomuxc = pctl_clk_iomux_snvs_gpr + 2, pctl_clk_iomuxc_gpr, pctl_clk_sim_cpu,
	pctl_clk_cxapbsyncbridge_slave, pctl_clk_tsc_dig, pctl_clk_p301_mx6qper1_bch, pctl_clk_pl301_mx6qper2_mainclk,
	pctl_clk_pwm1, pctl_clk_pwm2, pctl_clk_pwm3, pctl_clk_pwm4, pctl_clk_rawnand_u_bch_input_apb,
	pctl_clk_rawnand_u_gpmi_bch_input_bch, pctl_clk_rawnand_u_gpmi_bch_input_gpmi_io,
	pctl_clk_rawnand_u_gpmi_input_apb,

	/* CCM_CCGR5 */
	pctl_clk_rom, pctl_clk_sctr, pctl_clk_snvs_dryice, pctl_clk_sdma, pctl_clk_kpp, pctl_clk_wdog2,
	pctl_clk_spba, pctl_clk_spdif, pctl_clk_sim_main, pctl_clk_snvs_hp, pctl_clk_snvs_lp, pctl_clk_sai3,
	pctl_clk_uart1, pctl_clk_uart7, pctl_clk_sai1, pctl_clk_sai2,

	/* CCM_CCGR6 */
	pctl_clk_usboh3, pctl_clk_usdhc1, pctl_clk_usdhc2, pctl_clk_ipmux4 = pctl_clk_usdhc2 + 2,
	pctl_clk_eim_slow, pctl_clk_uart_debug, pctl_clk_uart8, pctl_clk_pwm8, pctl_clk_aips_tz3, pctl_clk_wdog3,
	pctl_clk_anadig, pctl_clk_i2c4_serial, pctl_clk_pwm5, pctl_clk_pwm6, pctl_clk_pwm7
};


/* IOMUX - GPR */
enum {
	/* IOMUXC_GPR_GPR0 */
	pctl_gpr_dmareq0 = 0, pctl_gpr_dmareq1, pctl_gpr_dmareq2, pctl_gpr_dmareq3, pctl_gpr_dmareq4, pctl_gpr_dmareq5,
	pctl_gpr_dmareq6, pctl_gpr_dmareq7, pctl_gpr_dmareq8, pctl_gpr_dmareq9, pctl_gpr_dmareq10, pctl_gpr_dmareq11,
	pctl_gpr_dmareq12, pctl_gpr_dmareq13, pctl_gpr_dmareq14, pctl_gpr_dmareq15, pctl_gpr_dmareq16, pctl_gpr_dmareq17,
	pctl_gpr_dmareq18, pctl_gpr_dmareq19, pctl_gpr_dmareq20, pctl_gpr_dmareq21, pctl_gpr_dmareq22,

	/* IOMUXC_GPR_GPR1 */
	pctl_gpr_act_cs0 = 32, pctl_gpr_addrs0, pctl_gpr_act_cs1 = 35, pctl_gpr_addrs1, pctl_gpr_act_cs2 = 38,
	pctl_gpr_addrs2, pctl_gpr_act_cs3 = 41, pctl_gpr_addrs3, pctl_gpr_gint = 44, pctl_gpr_enet1_clk,
	pctl_gpr_enet2_clk, pctl_gpr_usb_exp, pctl_gpr_add_ds, pctl_gpr_enet1_tx, pctl_gpr_enet2_tx, pctl_gpr_sai1_mclk,
	pctl_gpr_sai2_mclk, pctl_gpr_sai3_mclk, pctl_gpr_exc_mon, pctl_gpr_tzasc1, pctl_gpr_arma7_atb, pctl_gpr_armv7_ahb,

	/* IOMUXC_GPR_GPR2 */
	pctl_gpr_pxp_powersaving = 64, pctl_gpr_pxp_shutdown, pctl_gpr_pxp_deepsleep, pctl_gpr_pxp_lightsleep,
	pctl_gpr_lcdif1_powersaving, pctl_gpr_lcdif1_shutdown, pctl_gpr_lcdif1_deepsleep, pctl_gpr_lcdif1_lightsleep,
	pctl_gpr_lcdif2_powersaving, pctl_gpr_lcdif2_shutdown, pctl_gpr_lcdif2_deepsleep, pctl_gpr_lcdif2_lightsleep,
	pctl_gpr_l2_powersaving, pctl_gpr_l2_shutdown, pctl_gpr_l2_deepsleep, pctl_gpr_l2_lightsleep, pctl_gpr_mqs_clk_div,
	pctl_gpr_mqs_sw_rst = 88, pctl_gpr_mqs_en, pctl_gpr_mqs_oversample, pctl_gpr_dram_rst_bypass, pctl_gpr_dram_rst,
	pctl_gpr_dram_cke0, pctl_gpr_dram_cke1, pctl_gpr_dram_cke_bypass,

	/* IOMUXC_GPR_GPR3 */
	pctl_gpr_ocram_ctl, pctl_gpr_core_dbg = 109, pctl_gpr_ocram_status = 112,

	/* IOMUXC_GPR_GPR4 */
	pctl_gpr_sdma_stop_req = 128, pctl_gpr_can1_stop_req, pctl_gpr_can2_stop_req, pctl_gpr_enet1_stop_req,
	pctl_gpr_enet2_stop_req, pctl_gpr_sai1_stop_req, pctl_gpr_sai2_stop_req, pctl_gpr_sai3_stop_req,
	pctl_gpr_enet_ipg, pctl_gpr_sdma_stop_ack = 144, pctl_gpr_can1_stop_ack, pctl_gpr_can2_stop_ack,
	pctl_gpr_enet1_stop_ack, pctl_gpr_enet2_stop_ack, pctl_gpr_sai1_stop_ack, pctl_gpr_sai2_stop_ack,
	pctl_gpr_sai3_stop_ack, pctl_gpr_arm_wfi = 158, pctl_gpr_arm_wfe,

	/* IOMUXC_GPR_GPR5 */
	pctl_gpr_wdog1 = 166, pctl_gpr_wdog2, pctl_gpr_wdog3 = 180, pctl_gpr_gpt2_capn1 = 183, pctl_gpr_gpt2_capn2,
	pctl_gpr_enet1_event3n, pctl_gpr_enet2_event3n, pctl_gpr_vref_gpt1 = 188, pctl_gpr_vref_gpt2,
	pctl_gpr_ref_epit1, pctl_gpr_ref_epit2,

	/* IOMUXC_GPR_GPR9 */
	pctl_gpr_tzasc1_byp = 288,

	/* IOMUXC_GPR_GPR10 */
	pctl_gpr_dbg_en = 320, pctl_gpr_dbg_clk_en, pctl_gpr_sec_err_resp, pctl_gpr_ocram_tz_en = 330,
	pctl_gpr_ocram_tz_addr,

	/* IOMUXC_GPR_GPR14 */
	pctl_gpr_sm1 = 448, pctl_gpr_sm2
};


/* IOMUX - MUX */
enum {
	pctl_mux_boot_mode0 = 0, pctl_mux_boot_mode1, pctl_mux_tamper0, pctl_mux_tamper1, pctl_mux_tamper2,
	pctl_mux_tamper3, pctl_mux_tamper4, pctl_mux_tamper5, pctl_mux_tamper6, pctl_mux_tamper7, pctl_mux_tamper8,
	pctl_mux_tamper9, pctl_mux_jtag_mod = 17, pctl_mux_jtag_tms, pctl_mux_jtag_tdo, pctl_mux_jtag_tdi,
	pctl_mux_jtag_tck, pctl_mux_jtag_trst, pctl_mux_gpio1_00, pctl_mux_gpio1_01, pctl_mux_gpio1_02,
	pctl_mux_gpio1_03, pctl_mux_gpio1_04, pctl_mux_gpio1_05, pctl_mux_gpio1_06, pctl_mux_gpio1_07,
	pctl_mux_gpio1_08, pctl_mux_gpio1_09, pctl_mux_uart1_tx, pctl_mux_uart1_rx, pctl_mux_uart1_cts,
	pctl_mux_uart1_rts, pctl_mux_uart2_tx, pctl_mux_uart2_rx, pctl_mux_uart2_cts, pctl_mux_uart2_rts,
	pctl_mux_uart3_tx, pctl_mux_uart3_rx, pctl_mux_uart3_cts, pctl_mux_uart3_rts, pctl_mux_uart4_tx,
	pctl_mux_uart4_rx, pctl_mux_uart5_tx, pctl_mux_uart5_rx, pctl_mux_enet1_rx0, pctl_mux_enet1_rx1,
	pctl_mux_enet1_rxen, pctl_mux_enet1_tx0, pctl_mux_enet1_tx1, pctl_mux_enet1_txen, pctl_mux_enet1_txclk,
	pctl_mux_enet1_rxer, pctl_mux_enet2_rx0, pctl_mux_enet2_rx1, pctl_mux_enet2_rxen, pctl_mux_enet2_tx0,
	pctl_mux_enet2_tx1, pctl_mux_enet2_txen, pctl_mux_enet2_txclk, pctl_mux_enet2_rxer, pctl_mux_lcd_clk,
	pctl_mux_lcd_en, pctl_mux_lcd_hsync, pctl_mux_lcd_vsync, pctl_mux_lcd_rst, pctl_mux_lcd_d0, pctl_mux_lcd_d1,
	pctl_mux_lcd_d2, pctl_mux_lcd_d3, pctl_mux_lcd_d4, pctl_mux_lcd_d5, pctl_mux_lcd_d6, pctl_mux_lcd_d7,
	pctl_mux_lcd_d8, pctl_mux_lcd_d9, pctl_mux_lcd_d10, pctl_mux_lcd_d11, pctl_mux_lcd_d12, pctl_mux_lcd_d13,
	pctl_mux_lcd_d14, pctl_mux_lcd_d15, pctl_mux_lcd_d16, pctl_mux_lcd_d17, pctl_mux_lcd_d18, pctl_mux_lcd_d19,
	pctl_mux_lcd_d20, pctl_mux_lcd_d21, pctl_mux_lcd_d22, pctl_mux_lcd_d23, pctl_mux_nand_re, pctl_mux_nand_we,
	pctl_mux_nand_d0, pctl_mux_nand_d1, pctl_mux_nand_d2, pctl_mux_nand_d3, pctl_mux_nand_d4, pctl_mux_nand_d5,
	pctl_mux_nand_d6, pctl_mux_nand_d7, pctl_mux_nand_ale, pctl_mux_nand_wp, pctl_mux_nand_rdy,
	pctl_mux_nand_ce0, pctl_mux_nand_ce1, pctl_mux_nand_cle, pctl_mux_nand_dqs, pctl_mux_sd1_cmd,
	pctl_mux_sd1_clk, pctl_mux_sd1_d0, pctl_mux_sd1_d1, pctl_mux_sd1_d2, pctl_mux_sd1_d3, pctl_mux_csi_mclk,
	pctl_mux_csi_pclk, pctl_mux_csi_hsync, pctl_mux_csi_vsync, pctl_mux_csi_d0, pctl_mux_csi_d1,
	pctl_mux_csi_d2, pctl_mux_csi_d3, pctl_mux_csi_d4, pctl_mux_csi_d5, pctl_mux_csi_d6, pctl_mux_csi_d7
};


/* IOMUX - PAD */
enum {
	pctl_pad_test_mode = 0, pctl_pad_por, pctl_pad_onoff, pctl_pad_pmic_on, pctl_pad_pmic_stby, pctl_pad_boot0,
	pctl_pad_boot1, pctl_pad_tamper0, pctl_pad_tamper1, pctl_pad_tamper2, pctl_pad_tamper3, pctl_pad_tamper4,
	pctl_pad_tamper5, pctl_pad_tamper6, pctl_pad_tamper7, pctl_pad_tamper8, pctl_pad_tamper9,
	pctl_pad_jtag_mod = 180, pctl_pad_jtag_tms, pctl_pad_jtag_tdo, pctl_pad_jtag_tdi, pctl_pad_jtag_tck,
	pctl_pad_jtag_rst, pctl_pad_gpio1_00, pctl_pad_gpio1_01, pctl_pad_gpio1_02, pctl_pad_gpio1_03,
	pctl_pad_gpio1_04, pctl_pad_gpio1_05, pctl_pad_gpio1_06, pctl_pad_gpio1_07, pctl_pad_gpio1_08,
	pctl_pad_gpio1_09, pctl_pad_uart1_tx, pctl_pad_uart1_rx, pctl_pad_uart1_cts, pctl_pad_uart1_rts,
	pctl_pad_uart2_tx, pctl_pad_uart2_rx, pctl_pad_uart2_cts, pctl_pad_uart2_rts, pctl_pad_uart3_tx,
	pctl_pad_uart3_rx, pctl_pad_uart3_cts, pctl_pad_uart3_rts, pctl_pad_uart4_tx, pctl_pad_uart4_rx,
	pctl_pad_uart5_tx, pctl_pad_uart5_rx, pctl_pad_enet1_rx0, pctl_pad_enet1_rx1, pctl_pad_enet1_rxen,
	pctl_pad_enet1_tx0, pctl_pad_enet1_tx1, pctl_pad_enet1_txen, pctl_pad_enet1_txclk, pctl_pad_enet1_rxer,
	pctl_pad_enet2_rx0, pctl_pad_enet2_rx1, pctl_pad_enet2_rxen, pctl_pad_enet2_tx0, pctl_pad_enet2_tx1,
	pctl_pad_enet2_txen, pctl_pad_enet2_txclk, pctl_pad_enet2_rxer, pctl_pad_lcd_clk, pctl_pad_lcd_en,
	pctl_pad_lcd_hsync, pctl_pad_lcd_vsync, pctl_pad_lcd_rst, pctl_pad_lcd_d0, pctl_pad_lcd_d1, pctl_pad_lcd_d2,
	pctl_pad_lcd_d3, pctl_pad_lcd_d4, pctl_pad_lcd_d5, pctl_pad_lcd_d6, pctl_pad_lcd_d7, pctl_pad_lcd_d8,
	pctl_pad_lcd_d9, pctl_pad_lcd_d10, pctl_pad_lcd_d11, pctl_pad_lcd_d12, pctl_pad_lcd_d13, pctl_pad_lcd_d14,
	pctl_pad_lcd_d15, pctl_pad_lcd_d16, pctl_pad_lcd_d17, pctl_pad_lcd_d18, pctl_pad_lcd_d19, pctl_pad_lcd_d20,
	pctl_pad_lcd_d21, pctl_pad_lcd_d22, pctl_pad_lcd_d23, pctl_pad_nand_re, pctl_pad_nand_we, pctl_pad_nand_d0,
	pctl_pad_nand_d1, pctl_pad_nand_d2, pctl_pad_nand_d3, pctl_pad_nand_d4, pctl_pad_nand_d5, pctl_pad_nand_d6,
	pctl_pad_nand_d7, pctl_pad_nand_ale, pctl_pad_nand_wp, pctl_pad_nand_rdy, pctl_pad_nand_ce0, pctl_pad_nand_ce1,
	pctl_pad_nand_cle, pctl_pad_nand_dqs, pctl_pad_sd1_cmd, pctl_pad_sd1_clk, pctl_pad_sd1_d0, pctl_pad_sd1_d1,
	pctl_pad_sd1_d2, pctl_pad_sd1_d3, pctl_pad_csi_mclk, pctl_pad_csi_pclk, pctl_pad_csi_vsync, pctl_pad_csi_hsync,
	pctl_pad_csi_d0, pctl_pad_csi_d1, pctl_pad_csi_d2, pctl_pad_csi_d3, pctl_pad_csi_d4, pctl_pad_csi_d5,
	pctl_pad_csi_d6, pctl_pad_csi_d7
};


/* IOMUX - DAISY */
enum {
	pctl_isel_anatop = 302, pctl_isel_usb_otg2id, pctl_isel_ccm_pmicrdy, pctl_isel_csi_d2, pctl_isel_csi_d3,
	pctl_isel_csi_d5, pctl_isel_csi_d0, pctl_isel_csi_d1, pctl_isel_csi_d4, pctl_isel_csi_d6, pctl_isel_csi_d7,
	pctl_isel_csi_d8, pctl_isel_csi_d9, pctl_isel_csi_d10, pctl_isel_csi_d11, pctl_isel_csi_d12,
	pctl_isel_csi_d13, pctl_isel_csi_d14, pctl_isel_csi_d15, pctl_isel_csi_d16, pctl_isel_csi_d17,
	pctl_isel_csi_d18, pctl_isel_csi_d19, pctl_isel_csi_d20, pctl_isel_csi_d21, pctl_isel_csi_d22,
	pctl_isel_csi_d23, pctl_isel_csi_hsync, pctl_isel_csi_pclk, pctl_isel_csi_vsync, pctl_isel_csi_field,
	pctl_isel_ecspi1_sclk, pctl_isel_ecspi1_miso, pctl_isel_ecspi1_mosi, pctl_isel_ecspi1_ss0,
	pctl_isel_ecspi2_sclk, pctl_isel_ecspi2_miso, pctl_isel_ecspi2_mosi, pctl_isel_ecspi2_ss0,
	pctl_isel_ecspi3_sclk, pctl_isel_ecspi3_miso, pctl_isel_ecspi3_mosi, pctl_isel_ecspi3_ss0,
	pctl_isel_ecspi4_sclk, pctl_isel_ecspi4_miso, pctl_isel_ecspi4_mosi, pctl_isel_ecspi4_ss0,
	pctl_isel_enet1_refclk1, pctl_isel_enet1_mac0mdio, pctl_isel_enet2_refclk2, pctl_isel_enet2_mac0mdio,
	pctl_isel_flexcan1_rx, pctl_isel_flexcan2_rx, pctl_isel_gpt1_cap1, pctl_isel_gpt1_cap2, pctl_isel_gpt1_clksel,
	pctl_isel_gpt2_cap1, pctl_isel_gpt2_cap2, pctl_isel_gpt2_clksel, pctl_isel_i2c1_scl, pctl_isel_i2c1_sda,
	pctl_isel_i2c2_scl, pctl_isel_i2c2_sda, pctl_isel_i2c3_scl, pctl_isel_i2c3_sda, pctl_isel_i2c4_scl,
	pctl_isel_i2c4_sda, pctl_isel_kpp_col0, pctl_isel_kpp_col1, pctl_isel_kpp_col2, pctl_isel_kpp_row0,
	pctl_isel_kpp_row1, pctl_isel_kpp_row2, pctl_isel_lcd_busy, pctl_isel_sai1_mclk, pctl_isel_sai1_rx,
	pctl_isel_sai1_txclk, pctl_isel_sai1_txsync, pctl_isel_sai2_mclk, pctl_isel_sai2_rx, pctl_isel_sai2_txclk,
	pctl_isel_sai2_txsync, pctl_isel_sai3_mclk, pctl_isel_sai3_rx, pctl_isel_sai3_txclk, pctl_isel_sai3_txsync,
	pctl_isel_sdma_ev0, pctl_isel_sdma_ev1, pctl_isel_spdif_in, pctl_isel_spdif_clk, pctl_isel_uart1_rts,
	pctl_isel_uart1_rx, pctl_isel_uart2_rts, pctl_isel_uart2_rx, pctl_isel_uart3_rts, pctl_isel_uart3_rx,
	pctl_isel_uart4_rts, pctl_isel_uart4_rx, pctl_isel_uart5_rts, pctl_isel_uart5_rx, pctl_isel_uart6_rts,
	pctl_isel_uart6_rx, pctl_isel_uart7_rts, pctl_isel_uart7_rx, pctl_isel_uart8_rts, pctl_isel_uart8_rx,
	pctl_isel_usb_otg2oc, pctl_isel_usb_otgoc, pctl_isel_usdhc1_cd, pctl_isel_usdhc1_wp, pctl_isel_usdhc2_clk,
	pctl_isel_usdhc2_cd, pctl_isel_usdhc2_cmd, pctl_isel_usdhc2_d0, pctl_isel_usdhc2_d1, pctl_isel_usdhc2_d2,
	pctl_isel_usdhc2_d3, pctl_isel_usdhc2_d4, pctl_isel_usdhc2_d5, pctl_isel_usdhc2_d6, pctl_isel_usdhc2_d7,
	pctl_isel_usdhc2_wp
};


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_devclock = 0, pctl_iogpr, pctl_iomux, pctl_iopad, pctl_ioisel } type;

	union {
		struct {
			int dev;
			unsigned int state;
		} devclock;

		struct {
			int field;
			unsigned int val;
		} iogpr;

		struct {
			int mux;
			char sion;
			char mode;
		} iomux;

		struct {
			int pad;
			char hys;
			char pus;
			char pue;
			char pke;
			char ode;
			char speed;
			char dse;
			char sre;
		} iopad;

		struct {
			int isel;
			char daisy;
		} ioisel;
	};
} __attribute__((packed)) platformctl_t;

#endif
