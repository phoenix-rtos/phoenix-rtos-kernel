/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ZynqMP basic peripherals control functions
 *
 * Copyright 2021, 2024 Phoenix Systems
 * Author: Hubert Buczynski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_ARCH_ZYNQMP_H_
#define _PH_ARCH_ZYNQMP_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

#define PCTL_MIO_DRIVE_2mA     (0x0U)
#define PCTL_MIO_DRIVE_4mA     (0x1U)
#define PCTL_MIO_DRIVE_8mA     (0x2U)
#define PCTL_MIO_DRIVE_12mA    (0x3U)
#define PCTL_MIO_SCHMITT_nCMOS (0x1U << 3)
#define PCTL_MIO_PULL_UP_nDOWN (0x1U << 4)
#define PCTL_MIO_PULL_ENABLE   (0x1U << 5)
#define PCTL_MIO_SLOW_nFAST    (0x1U << 6)
#define PCTL_MIO_TRI_ENABLE    (0x1U << 7)


/* clang-format off */
/* Devices' clocks controllers */
enum {
	pctl_devclock_lpd_usb3_dual = 0x12,
	pctl_devclock_lpd_gem0,
	pctl_devclock_lpd_gem1,
	pctl_devclock_lpd_gem2,
	pctl_devclock_lpd_gem3,
	pctl_devclock_lpd_usb0_bus,
	pctl_devclock_lpd_usb1_bus,
	pctl_devclock_lpd_qspi,
	pctl_devclock_lpd_sdio0,
	pctl_devclock_lpd_sdio1,
	pctl_devclock_lpd_uart0,
	pctl_devclock_lpd_uart1,
	pctl_devclock_lpd_spi0,
	pctl_devclock_lpd_spi1,
	pctl_devclock_lpd_can0,
	pctl_devclock_lpd_can1,
	pctl_devclock_lpd_cpu_r5 = 0x24,
	pctl_devclock_lpd_iou_switch = 0x27,
	pctl_devclock_lpd_csu_pll,
	pctl_devclock_lpd_pcap,
	pctl_devclock_lpd_lpd_switch,
	pctl_devclock_lpd_lpd_lsbus,
	pctl_devclock_lpd_dbg_lpd,
	pctl_devclock_lpd_nand,
	pctl_devclock_lpd_lpd_dma,
	pctl_devclock_lpd_pl0 = 0x30,
	pctl_devclock_lpd_pl1,
	pctl_devclock_lpd_pl2,
	pctl_devclock_lpd_pl3,
	pctl_devclock_lpd_gem_tsu = 0x40,
	pctl_devclock_lpd_dll,
	pctl_devclock_lpd_pssysmon,
	pctl_devclock_lpd_i2c0 = 0x48,
	pctl_devclock_lpd_i2c1,
	pctl_devclock_lpd_timestamp,
	pctl_devclock_fpd_acpu = 0x50 + 0x18,
	pctl_devclock_fpd_dbg_trace,
	pctl_devclock_fpd_dbg_fpd,
	pctl_devclock_fpd_dp_video = 0x50 + 0x1c,
	pctl_devclock_fpd_dp_audio,
	pctl_devclock_fpd_dp_stc = 0x50 + 0x1f,
	pctl_devclock_fpd_ddr,
	pctl_devclock_fpd_gpu,
	pctl_devclock_fpd_sata = 0x50 + 0x28,
	pctl_devclock_fpd_pcie = 0x50 + 0x2d,
	pctl_devclock_fpd_fpd_dma,
	pctl_devclock_fpd_dpdma,
	pctl_devclock_fpd_topsw_main,
	pctl_devclock_fpd_topsw_lsbus,
	pctl_devclock_fpd_dbg_tstmp = 0x50 + 0x3e,
};

/* Devices' reset controllers */
enum {
	pctl_devreset_lpd_gem0 = 0x0, pctl_devreset_lpd_gem1, pctl_devreset_lpd_gem2, pctl_devreset_lpd_gem3, pctl_devreset_lpd_qspi,
	pctl_devreset_lpd_uart0, pctl_devreset_lpd_uart1, pctl_devreset_lpd_spi0, pctl_devreset_lpd_spi1, pctl_devreset_lpd_sdio0,
	pctl_devreset_lpd_sdio1, pctl_devreset_lpd_can0, pctl_devreset_lpd_can1, pctl_devreset_lpd_i2c0, pctl_devreset_lpd_i2c1,
	pctl_devreset_lpd_ttc0, pctl_devreset_lpd_ttc1, pctl_devreset_lpd_ttc2, pctl_devreset_lpd_ttc3, pctl_devreset_lpd_swdt,
	pctl_devreset_lpd_nand, pctl_devreset_lpd_lpd_dma, pctl_devreset_lpd_gpio, pctl_devreset_lpd_iou_cc,
	pctl_devreset_lpd_timestamp, pctl_devreset_lpd_rpu_r50, pctl_devreset_lpd_rpu_r51, pctl_devreset_lpd_rpu_amba,
	pctl_devreset_lpd_ocm, pctl_devreset_lpd_rpu_pge, pctl_devreset_lpd_usb0_corereset, pctl_devreset_lpd_usb1_corereset,
	pctl_devreset_lpd_usb0_hiberreset, pctl_devreset_lpd_usb1_hiberreset, pctl_devreset_lpd_usb0_apb,
	pctl_devreset_lpd_usb1_apb, pctl_devreset_lpd_ipi, pctl_devreset_lpd_apm, pctl_devreset_lpd_rtc, pctl_devreset_lpd_sysmon,
	pctl_devreset_lpd_s_axi_lpd, pctl_devreset_lpd_lpd_swdt, pctl_devreset_lpd_fpd, pctl_devreset_lpd_dbg_fpd,
	pctl_devreset_lpd_dbg_lpd, pctl_devreset_lpd_rpu_dbg0, pctl_devreset_lpd_rpu_dbg1, pctl_devreset_lpd_dbg_ack,
	pctl_devreset_fpd_sata, pctl_devreset_fpd_gt, pctl_devreset_fpd_gpu, pctl_devreset_fpd_gpu_pp0, pctl_devreset_fpd_gpu_pp1,
	pctl_devreset_fpd_fpd_dma, pctl_devreset_fpd_s_axi_hpc_0_fpd, pctl_devreset_fpd_s_axi_hpc_1_fpd,
	pctl_devreset_fpd_s_axi_hp_0_fpd, pctl_devreset_fpd_s_axi_hp_1_fpd, pctl_devreset_fpd_s_axi_hpc_2_fpd,
	pctl_devreset_fpd_s_axi_hpc_3_fpd, pctl_devreset_fpd_swdt, pctl_devreset_fpd_dp, pctl_devreset_fpd_pcie_ctrl,
	pctl_devreset_fpd_pcie_bridge, pctl_devreset_fpd_pcie_cfg, pctl_devreset_fpd_acpu0, pctl_devreset_fpd_acpu1,
	pctl_devreset_fpd_acpu2, pctl_devreset_fpd_acpu3, pctl_devreset_fpd_apu_l2, pctl_devreset_fpd_acpu0_pwron,
	pctl_devreset_fpd_acpu1_pwron, pctl_devreset_fpd_acpu2_pwron, pctl_devreset_fpd_acpu3_pwron, pctl_devreset_fpd_ddr_apm,
	pctl_devreset_fpd_ddr_reserved,
};


enum {
	pctl_mio_pin_00 = 0, pctl_mio_pin_01, pctl_mio_pin_02, pctl_mio_pin_03, pctl_mio_pin_04, pctl_mio_pin_05, pctl_mio_pin_06,
	pctl_mio_pin_07, pctl_mio_pin_08, pctl_mio_pin_09, pctl_mio_pin_10, pctl_mio_pin_11, pctl_mio_pin_12, pctl_mio_pin_13,
	pctl_mio_pin_14, pctl_mio_pin_15, pctl_mio_pin_16, pctl_mio_pin_17, pctl_mio_pin_18, pctl_mio_pin_19, pctl_mio_pin_20,
	pctl_mio_pin_21, pctl_mio_pin_22, pctl_mio_pin_23, pctl_mio_pin_24, pctl_mio_pin_25, pctl_mio_pin_26, pctl_mio_pin_27,
	pctl_mio_pin_28, pctl_mio_pin_29, pctl_mio_pin_30, pctl_mio_pin_31, pctl_mio_pin_32, pctl_mio_pin_33, pctl_mio_pin_34,
	pctl_mio_pin_35, pctl_mio_pin_36, pctl_mio_pin_37, pctl_mio_pin_38, pctl_mio_pin_39, pctl_mio_pin_40, pctl_mio_pin_41,
	pctl_mio_pin_42, pctl_mio_pin_43, pctl_mio_pin_44, pctl_mio_pin_45, pctl_mio_pin_46, pctl_mio_pin_47, pctl_mio_pin_48,
	pctl_mio_pin_49, pctl_mio_pin_50, pctl_mio_pin_51, pctl_mio_pin_52, pctl_mio_pin_53, pctl_mio_pin_54, pctl_mio_pin_55,
	pctl_mio_pin_56, pctl_mio_pin_57, pctl_mio_pin_58, pctl_mio_pin_59, pctl_mio_pin_60, pctl_mio_pin_61, pctl_mio_pin_62,
	pctl_mio_pin_63, pctl_mio_pin_64, pctl_mio_pin_65, pctl_mio_pin_66, pctl_mio_pin_67, pctl_mio_pin_68, pctl_mio_pin_69,
	pctl_mio_pin_70, pctl_mio_pin_71, pctl_mio_pin_72, pctl_mio_pin_73, pctl_mio_pin_74, pctl_mio_pin_75, pctl_mio_pin_76,
	pctl_mio_pin_77,
};
/* clang-format on */


typedef struct {
	/* clang-format off */
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_devclock = 0, pctl_mioclock, pctl_mio, pctl_devreset, pctl_reboot } type;
	/* clang-format on */
	union {
		struct {
			__u32 dev;
			__u8 src;    /* 0, 2, 3 for most devices, 0, 2, 3, 4 for pctl_devclock_lpd_timestamp */
			__u8 div0;   /* 0 ~ 63 */
			__u8 div1;   /* 0 ~ 63 if supported by selected generator, otherwise 0 */
			__u8 active; /* 0, 1 for most devices, some have additional active bits  */
		} devclock;

		struct {
			__u32 pin;
			__u8 l3;
			__u8 l2;
			__u8 l1;
			__u8 l0;
			__u8 config;
		} mio;

		struct {
			__u32 dev;
			__u32 state;
		} devreset;

		struct {
			__u32 magic;
			__u32 reason;
		} reboot;
	};
} __attribute__((packed)) platformctl_t;

#endif
