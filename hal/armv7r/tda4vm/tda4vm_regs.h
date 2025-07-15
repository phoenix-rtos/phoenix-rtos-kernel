/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TDA4VM register definitions
 * Based on J721E DRA829/TDA4VM/AM68P Processors Silicon Revision 1.1
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _TDA4VM_REG_DEFS_H_
#define _TDA4VM_REG_DEFS_H_

#include "hal/types.h"

/* Magic values to unlock writing PLL configuration */
#define PLL_LOCKKEY_0 0x68EF3490
#define PLL_LOCKKEY_1 0xD172BC5A

#define CTRL_MMR0_BASE_ADDR                ((volatile u32 *)0x00100000)
#define PLLCTRL0_BASE_ADDR                 ((volatile u32 *)0x00410000)
#define MAIN_PLL_BASE_ADDR                 ((volatile u32 *)0x00680000)
#define INTR_ROUTER_MAIN2MCU_LVL_BASE_ADDR ((volatile u32 *)0x00a10000)
#define INTR_ROUTER_MAIN2MCU_PLS_BASE_ADDR ((volatile u32 *)0x00a20000)
#define INTR_ROUTER_MCU_NAVSS0_BASE_ADDR   ((volatile u32 *)0x28540000)
#define INTR_ROUTER_NAVSS0_BASE_ADDR       ((volatile u32 *)0x310e0000)
#define MCU_PLL_BASE_ADDR                  ((volatile u32 *)0x40d00000)
#define CTRLMMR_MCU_BASE_ADDR              ((volatile u32 *)0x40f00000)
#define MCU_ARMSS_RAT_BASE_ADDR            ((volatile u32 *)0x40f90000)
#define WKUP_PLLCTRL0_BASE_ADDR            ((volatile u32 *)0x42010000)
#define INTR_ROUTER_WKUP_GPIOMUX_BASE_ADDR ((volatile u32 *)0x42200000)
#define CTRLMMR_WKUP_BASE_ADDR             ((volatile u32 *)0x43000000)


enum r5fss_rat_regs {
	r5fss_rat_reg_pid = (0x0 / 4),                         /* Revision register */
	r5fss_rat_reg_config = (0x4 / 4),                      /* Config register */
	r5fss_rat_reg_ctrl_0 = (0x20 / 4),                     /* Region control register */
	r5fss_rat_reg_base_0 = (0x24 / 4),                     /* Region base register */
	r5fss_rat_reg_trans_l_0 = (0x28 / 4),                  /* Region translated lower address register */
	r5fss_rat_reg_trans_u_0 = (0x2c / 4),                  /* Region translated upper address register */
	r5fss_rat_reg_destination_id = (0x804 / 4),            /* Destination ID register */
	r5fss_rat_reg_exception_logging_control = (0x820 / 4), /* Exception logging control register */
	r5fss_rat_reg_exception_logging_header0 = (0x824 / 4), /* Exception logging header 0 register */
	r5fss_rat_reg_exception_logging_header1 = (0x828 / 4), /* Exception logging header 1 register */
	r5fss_rat_reg_exception_logging_data0 = (0x82c / 4),   /* Exception logging data 0 register */
	r5fss_rat_reg_exception_logging_data1 = (0x830 / 4),   /* Exception logging data 1 register */
	r5fss_rat_reg_exception_logging_data2 = (0x834 / 4),   /* Exception logging data 2 register */
	r5fss_rat_reg_exception_logging_data3 = (0x838 / 4),   /* Exception logging data 3 register */
	r5fss_rat_reg_exception_pend_set = (0x840 / 4),        /* Exception logging interrupt pending set register */
	r5fss_rat_reg_exception_pend_clear = (0x844 / 4),      /* Exception logging interrupt pending clear register */
	r5fss_rat_reg_exception_enable_set = (0x848 / 4),      /* Exception logging interrupt enable set register */
	r5fss_rat_reg_exception_enable_clear = (0x84c / 4),    /* Exception logging interrupt enable clear register */
	r5fss_rat_reg_eoi_reg = (0x850 / 4),                   /* EOI register */
};


enum pll_regs {
	pll_reg_pid = (0x0 / 4),          /* Peripheral Identification */
	pll_reg_cfg = (0x8 / 4),          /* PLL Configuration */
	pll_reg_lockkey0 = (0x10 / 4),    /* Lock Key 0 */
	pll_reg_lockkey1 = (0x14 / 4),    /* Lock Key 1 */
	pll_reg_ctrl = (0x20 / 4),        /* Control */
	pll_reg_stat = (0x24 / 4),        /* Status */
	pll_reg_freq_ctrl0 = (0x30 / 4),  /* Frequency control 0 */
	pll_reg_freq_ctrl1 = (0x34 / 4),  /* Frequency control 1 */
	pll_reg_div_ctrl = (0x38 / 4),    /* Output Clock Divider */
	pll_reg_ss_ctrl = (0x40 / 4),     /* Spread spectrum control */
	pll_reg_ss_spread = (0x44 / 4),   /* Spread spectrum parameters */
	pll_reg_hsdiv_ctrl0 = (0x80 / 4), /* High-speed divider control */
};

enum pllctrl_regs {
	pllctrl_reg_pid = 0,               /* Peripheral Identification */
	pllctrl_reg_pllctl = (0x100 / 4),  /* PLL control register */
	pllctrl_reg_plldiv1 = (0x118 / 4), /* PLL controller divider1 control register */
	pllctrl_reg_plldiv2 = (0x11c / 4), /* PLL controller divider2 control register */
	pllctrl_reg_pllcmd = (0x138 / 4),  /* PLL Controller command register */
	pllctrl_reg_pllstat = (0x13c / 4), /* PLL Controller status register */
	pllctrl_reg_alnctl = (0x140 / 4),  /* PLL Controller clock align control register */
	pllctrl_reg_dchange = (0x144 / 4), /* PLLDIV ratio change register */
};


/* TODO: add rest of registers from this module */
enum ctrlmmr_wkup_regs {
	ctrlmmr_wkup_reg_dbounce_cfg1 = (0x4084 / 4),
	ctrlmmr_wkup_reg_dbounce_cfg2,
	ctrlmmr_wkup_reg_dbounce_cfg3,
	ctrlmmr_wkup_reg_dbounce_cfg4,
	ctrlmmr_wkup_reg_dbounce_cfg5,
	ctrlmmr_wkup_reg_dbounce_cfg6,
	ctrlmmr_wkup_reg_mcu_obsclk_ctrl = (0x8000 / 4),   /* Observe Clock Output Control Register */
	ctrlmmr_wkup_reg_hfosc1_ctrl = (0x8014 / 4),       /* Oscillator1 Control Register */
	ctrlmmr_wkup_reg_lfxosc_ctrl = (0x8030 / 4),       /* Low Frequency Oscillator Control Register */
	ctrlmmr_wkup_reg_lfxosc_trim = (0x8034 / 4),       /* Low Frequency Oscillator Trim Register */
	ctrlmmr_wkup_reg_mcu_pll_clksel = (0x8050 / 4),    /* MCU PLL Source Clock Select Register */
	ctrlmmr_wkup_reg_per_clksel = (0x8060 / 4),        /* WKUP Peripheral Clock Select Register */
	ctrlmmr_wkup_reg_usart_clksel = (0x8064 / 4),      /* WKUP USART0 Clock Select Register */
	ctrlmmr_wkup_reg_gpio_clksel = (0x8070 / 4),       /* WKUP GPIO Clock Select Register */
	ctrlmmr_wkup_reg_main_pll0_clksel = (0x8080 / 4),  /* MAIN PLL0 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll1_clksel = (0x8084 / 4),  /* MAIN PLL1 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll2_clksel = (0x8088 / 4),  /* MAIN PLL2 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll3_clksel = (0x808c / 4),  /* MAIN PLL3 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll4_clksel = (0x8090 / 4),  /* MAIN PLL4 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll5_clksel = (0x8094 / 4),  /* MAIN PLL5 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll6_clksel = (0x8098 / 4),  /* MAIN PLL6 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll7_clksel = (0x809c / 4),  /* MAIN PLL7 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll8_clksel = (0x80a0 / 4),  /* MAIN PLL8 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll12_clksel = (0x80b0 / 4), /* MAIN PLL12 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll13_clksel = (0x80b4 / 4), /* MAIN PLL13 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll14_clksel = (0x80b8 / 4), /* MAIN PLL14 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll15_clksel = (0x80bc / 4), /* MAIN PLL15 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll16_clksel = (0x80c0 / 4), /* MAIN PLL16 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll17_clksel = (0x80c4 / 4), /* MAIN PLL17 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll18_clksel = (0x80c8 / 4), /* MAIN PLL18 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll19_clksel = (0x80cc / 4), /* MAIN PLL19 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll23_clksel = (0x80dc / 4), /* MAIN PLL23 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll24_clksel = (0x80e0 / 4), /* MAIN PLL24 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_pll25_clksel = (0x80e4 / 4), /* MAIN PLL25 Source Clock Select Register */
	ctrlmmr_wkup_reg_main_sysclk_ctrl = (0x8100 / 4),  /* MAIN System Clock Control Register */
	ctrlmmr_wkup_reg_mcu_spi0_clksel = (0x8110 / 4),   /* MCU_SPI Clock Select Register */
	ctrlmmr_wkup_reg_mcu_spi1_clksel = (0x8114 / 4),   /* MCU_SPI Clock Select Register */
	ctrlmmr_wkup_reg_mcu_warm_rst_ctrl = (0x1817c / 4),
	ctrlmmr_wkup_reg_padconfig0 = (0x1c000 / 4),
};


enum ctrlmmr_mcu_regs {
	ctrlmmr_mcu_reg_pid = (0x0 / 4),                /* Peripheral Identification Register */
	ctrlmmr_mcu_reg_mmr_cfg1 = (0x8 / 4),           /* Configuration register 1 */
	ctrlmmr_mcu_reg_ipc_set0 = (0x100 / 4),         /* IPC Generation Register 0 */
	ctrlmmr_mcu_reg_ipc_set1 = (0x104 / 4),         /* IPC Generation Register 1 */
	ctrlmmr_mcu_reg_ipc_set8 = (0x120 / 4),         /* IPC Generation Register 8 */
	ctrlmmr_mcu_reg_ipc_clr0 = (0x180 / 4),         /* IPC Acknowledge Register 0 */
	ctrlmmr_mcu_reg_ipc_clr1 = (0x184 / 4),         /* IPC Acknowledge Register 1 */
	ctrlmmr_mcu_reg_ipc_clr8 = (0x1a0 / 4),         /* IPC Acknowledge Register 8 */
	ctrlmmr_mcu_reg_mac_id0 = (0x200 / 4),          /* MAC Address Lo register */
	ctrlmmr_mcu_reg_mac_id1 = (0x204 / 4),          /* MAC Address Hi Register */
	ctrlmmr_mcu_reg_lock0_kick0 = (0x1008 / 4),     /* Partition 0 Lock Key 0 Register */
	ctrlmmr_mcu_reg_lock0_kick1 = (0x100c / 4),     /* Partition 0 Lock Key 1 Register */
	ctrlmmr_mcu_reg_intr_raw_stat = (0x1010 / 4),   /* Interrupt Raw Status Register */
	ctrlmmr_mcu_reg_intr_stat_clr = (0x1014 / 4),   /* Interrupt Status and Clear Register */
	ctrlmmr_mcu_reg_intr_en_set = (0x1018 / 4),     /* Interrupt Enable Set Register */
	ctrlmmr_mcu_reg_intr_en_clr = (0x101c / 4),     /* Interrupt Enable Clear Register */
	ctrlmmr_mcu_reg_eoi = (0x1020 / 4),             /* End of Interrupt Register */
	ctrlmmr_mcu_reg_fault_addr = (0x1024 / 4),      /* Fault Address Register */
	ctrlmmr_mcu_reg_fault_type = (0x1028 / 4),      /* Fault Type Register */
	ctrlmmr_mcu_reg_fault_attr = (0x102c / 4),      /* Fault Attribute Register */
	ctrlmmr_mcu_reg_fault_clr = (0x1030 / 4),       /* Fault Clear Register */
	ctrlmmr_mcu_reg_p0_claim0 = (0x1100 / 4),       /* Partition 0 Claim Register 0 */
	ctrlmmr_mcu_reg_p0_claim1 = (0x1104 / 4),       /* Partition 0 Claim Register 1 */
	ctrlmmr_mcu_reg_p0_claim2 = (0x1108 / 4),       /* Partition 0 Claim Register 2 */
	ctrlmmr_mcu_reg_p0_claim3 = (0x110c / 4),       /* Partition 0 Claim Register 3 */
	ctrlmmr_mcu_reg_p0_claim4 = (0x1110 / 4),       /* Partition 0 Claim Register 4 */
	ctrlmmr_mcu_reg_msmc_cfg = (0x4030 / 4),        /* MSMC Configuration Register */
	ctrlmmr_mcu_reg_enet_ctrl = (0x4040 / 4),       /* MCU Ethernet Port1 Control Register */
	ctrlmmr_mcu_reg_spi1_ctrl = (0x4060 / 4),       /* MCU SPI1 Connectivity Control Register */
	ctrlmmr_mcu_reg_i3c0_ctrl0 = (0x4070 / 4),      /* MCU I3C0 Control Register 0 */
	ctrlmmr_mcu_reg_i3c0_ctrl1 = (0x4074 / 4),      /* MCU I3C0 Control Register 1 */
	ctrlmmr_mcu_reg_i3c1_ctrl0 = (0x4078 / 4),      /* MCU I3C1 Control Register 0 */
	ctrlmmr_mcu_reg_i3c1_ctrl1 = (0x407c / 4),      /* MCU I3C1 Control Register 1 */
	ctrlmmr_mcu_reg_i2c0_ctrl = (0x4080 / 4),       /* MCU I2C0 Control Register */
	ctrlmmr_mcu_reg_fss_ctrl = (0x40a0 / 4),        /* Flash Subsystem Control Register */
	ctrlmmr_mcu_reg_adc0_ctrl = (0x40b0 / 4),       /* MCU_ADC0 Control Register */
	ctrlmmr_mcu_reg_adc1_ctrl = (0x40b4 / 4),       /* MCU_ADC1 Control Register */
	ctrlmmr_mcu_reg_timer0_ctrl = (0x4200 / 4),     /* MCU_TIMER0 Control Register */
	ctrlmmr_mcu_reg_timer1_ctrl = (0x4204 / 4),     /* MCU_TIMER1 Control Register */
	ctrlmmr_mcu_reg_timer2_ctrl = (0x4208 / 4),     /* MCU_TIMER2 Control Register */
	ctrlmmr_mcu_reg_timer3_ctrl = (0x420c / 4),     /* MCU_TIMER3 Control Register */
	ctrlmmr_mcu_reg_timer4_ctrl = (0x4210 / 4),     /* MCU_TIMER4 Control Register */
	ctrlmmr_mcu_reg_timer5_ctrl = (0x4214 / 4),     /* MCU_TIMER5 Control Register */
	ctrlmmr_mcu_reg_timer6_ctrl = (0x4218 / 4),     /* MCU_TIMER6 Control Register */
	ctrlmmr_mcu_reg_timer7_ctrl = (0x421c / 4),     /* MCU_TIMER7 Control Register */
	ctrlmmr_mcu_reg_timer8_ctrl = (0x4220 / 4),     /* MCU_TIMER8 Control Register */
	ctrlmmr_mcu_reg_timer9_ctrl = (0x4224 / 4),     /* MCU_TIMER9 Control Register */
	ctrlmmr_mcu_reg_timerio0_ctrl = (0x4280 / 4),   /* MCU_TIMERIO0 Control Register */
	ctrlmmr_mcu_reg_timerio1_ctrl = (0x4284 / 4),   /* MCU_TIMERIO1 Control Register */
	ctrlmmr_mcu_reg_timerio2_ctrl = (0x4288 / 4),   /* MCU_TIMERIO2 Control Register */
	ctrlmmr_mcu_reg_timerio3_ctrl = (0x428c / 4),   /* MCU_TIMERIO3 Control Register */
	ctrlmmr_mcu_reg_timerio4_ctrl = (0x4290 / 4),   /* MCU_TIMERIO4 Control Register */
	ctrlmmr_mcu_reg_timerio5_ctrl = (0x4294 / 4),   /* MCU_TIMERIO5 Control Register */
	ctrlmmr_mcu_reg_timerio6_ctrl = (0x4298 / 4),   /* MCU_TIMERIO6 Control Register */
	ctrlmmr_mcu_reg_timerio7_ctrl = (0x429c / 4),   /* MCU_TIMERIO7 Control Register */
	ctrlmmr_mcu_reg_timerio8_ctrl = (0x42a0 / 4),   /* MCU_TIMERIO8 Control Register */
	ctrlmmr_mcu_reg_timerio9_ctrl = (0x42a4 / 4),   /* MCU_TIMERIO9 Control Register */
	ctrlmmr_mcu_reg_lock1_kick0 = (0x5008 / 4),     /* Partition 1 Lock Key 0 Register */
	ctrlmmr_mcu_reg_lock1_kick1 = (0x500c / 4),     /* Partition 1 Lock Key 1 Register */
	ctrlmmr_mcu_reg_p1_claim0 = (0x5100 / 4),       /* Partition 1 Claim Register 0 */
	ctrlmmr_mcu_reg_p1_claim1 = (0x5104 / 4),       /* Partition 1 Claim Register 1 */
	ctrlmmr_mcu_reg_p1_claim2 = (0x5108 / 4),       /* Partition 1 Claim Register 2 */
	ctrlmmr_mcu_reg_p1_claim3 = (0x510c / 4),       /* Partition 1 Claim Register 3 */
	ctrlmmr_mcu_reg_p1_claim4 = (0x5110 / 4),       /* Partition 1 Claim Register 4 */
	ctrlmmr_mcu_reg_p1_claim5 = (0x5114 / 4),       /* Partition 1 Claim Register 5 */
	ctrlmmr_mcu_reg_clkout0_ctrl = (0x8010 / 4),    /* MCU_CLKOUT0 Control Register */
	ctrlmmr_mcu_reg_efuse_clksel = (0x8018 / 4),    /* MCU eFuse Controller Clock Select Register */
	ctrlmmr_mcu_reg_mcan0_clksel = (0x8020 / 4),    /* MCU_MCAN Clock Select Register */
	ctrlmmr_mcu_reg_mcan1_clksel = (0x8024 / 4),    /* MCU_MCAN Clock Select Register */
	ctrlmmr_mcu_reg_ospi0_clksel = (0x8030 / 4),    /* MCU_OSPI Clock Select Register */
	ctrlmmr_mcu_reg_ospi1_clksel = (0x8034 / 4),    /* MCU_OSPI Clock Select Register */
	ctrlmmr_mcu_reg_adc0_clksel = (0x8040 / 4),     /* MCU_ADC Clock Select Register */
	ctrlmmr_mcu_reg_adc1_clksel = (0x8044 / 4),     /* MCU_ADC Clock Select Register */
	ctrlmmr_mcu_reg_enet_clksel = (0x8050 / 4),     /* MCU Ethernet Port1 Clock Select Register */
	ctrlmmr_mcu_reg_r5_core0_clksel = (0x8080 / 4), /* MCU R5 Core 0 Clock Select Register */
	ctrlmmr_mcu_reg_timer0_clksel = (0x8100 / 4),   /* MCU_TIMER0 Clock Select Register */
	ctrlmmr_mcu_reg_timer1_clksel = (0x8104 / 4),   /* MCU_TIMER1 Clock Select Register */
	ctrlmmr_mcu_reg_timer2_clksel = (0x8108 / 4),   /* MCU_TIMER2 Clock Select Register */
	ctrlmmr_mcu_reg_timer3_clksel = (0x810c / 4),   /* MCU_TIMER3 Clock Select Register */
	ctrlmmr_mcu_reg_timer4_clksel = (0x8110 / 4),   /* MCU_TIMER4 Clock Select Register */
	ctrlmmr_mcu_reg_timer5_clksel = (0x8114 / 4),   /* MCU_TIMER5 Clock Select Register */
	ctrlmmr_mcu_reg_timer6_clksel = (0x8118 / 4),   /* MCU_TIMER6 Clock Select Register */
	ctrlmmr_mcu_reg_timer7_clksel = (0x811c / 4),   /* MCU_TIMER7 Clock Select Register */
	ctrlmmr_mcu_reg_timer8_clksel = (0x8120 / 4),   /* MCU_TIMER8 Clock Select Register */
	ctrlmmr_mcu_reg_timer9_clksel = (0x8124 / 4),   /* MCU_TIMER9 Clock Select Register */
	ctrlmmr_mcu_reg_rti0_clksel = (0x8180 / 4),     /* MCU_RTI[0:0] Clock Select Register */
	ctrlmmr_mcu_reg_rti1_clksel = (0x8184 / 4),     /* MCU_RTI[0:0] Clock Select Register */
	ctrlmmr_mcu_reg_usart_clksel = (0x81c0 / 4),    /* MCU_USART0 Clock Select Register */
	ctrlmmr_mcu_reg_lock2_kick0 = (0x9008 / 4),     /* Partition 2 Lock Key 0 Register */
	ctrlmmr_mcu_reg_lock2_kick1 = (0x900c / 4),     /* Partition 2 Lock Key 1 Register */
	ctrlmmr_mcu_reg_p2_claim0 = (0x9100 / 4),       /* Partition 2 Claim Register 0 */
	ctrlmmr_mcu_reg_p2_claim1 = (0x9104 / 4),       /* Partition 2 Claim Register 1 */
	ctrlmmr_mcu_reg_p2_claim2 = (0x9108 / 4),       /* Partition 2 Claim Register 2 */
	ctrlmmr_mcu_reg_p2_claim3 = (0x910c / 4),       /* Partition 2 Claim Register 3 */
	ctrlmmr_mcu_reg_lbist_ctrl = (0xc000 / 4),      /* MCU_Pulsar Logic BIST Control Register */
	ctrlmmr_mcu_reg_lbist_patcount = (0xc004 / 4),  /* MCU_Pulsar Logic BIST Pattern Count Register */
	ctrlmmr_mcu_reg_lbist_seed0 = (0xc008 / 4),     /* MCU_Pulsar Logic BIST Seed0 Register */
	ctrlmmr_mcu_reg_lbist_seed1 = (0xc00c / 4),     /* MCU_Pulsar Logic BIST Seed1 Register */
	ctrlmmr_mcu_reg_lbist_spare0 = (0xc010 / 4),    /* MCU_Pulsar Logic BIST Spare0 Register */
	ctrlmmr_mcu_reg_lbist_spare1 = (0xc014 / 4),    /* MCU_Pulsar Logic BIST Spare1 Register */
	ctrlmmr_mcu_reg_lbist_stat = (0xc018 / 4),      /* MCU_Pulsar Logic BIST Status Register */
	ctrlmmr_mcu_reg_lbist_misr = (0xc01c / 4),      /* MCU_Pulsar Logic BIST MISR Register */
	ctrlmmr_mcu_reg_lbist_sig = (0xc280 / 4),       /* MCU Pulsar Logic BIST MISR Signature Register */
	ctrlmmr_mcu_reg_lock3_kick0 = (0xd008 / 4),     /* Partition 3 Lock Key 0 Register */
	ctrlmmr_mcu_reg_lock3_kick1 = (0xd00c / 4),     /* Partition 3 Lock Key 1 Register */
	ctrlmmr_mcu_reg_p3_claim0 = (0xd100 / 4),       /* Partition 3 Claim Register 0 */
	ctrlmmr_mcu_reg_p3_claim1 = (0xd104 / 4),       /* Partition 3 Claim Register 1 */
	ctrlmmr_mcu_reg_p3_claim2 = (0xd108 / 4),       /* Partition 3 Claim Register 2 */
	ctrlmmr_mcu_reg_p3_claim3 = (0xd10c / 4),       /* Partition 3 Claim Register 3 */
	ctrlmmr_mcu_reg_p3_claim4 = (0xd110 / 4),       /* Partition 3 Claim Register 4 */
	ctrlmmr_mcu_reg_p3_claim5 = (0xd114 / 4),       /* Partition 3 Claim Register 5 */
};

enum ctrlmmr_regs {
	ctrlmmr_reg_pid = (0x0 / 4),                      /* Peripheral Identification Register */
	ctrlmmr_reg_mmr_cfg1 = (0x8 / 4),                 /* Configuration register 1 */
	ctrlmmr_reg_main_devstat = (0x30 / 4),            /* MAIN Domain Device Status Register */
	ctrlmmr_reg_main_bootcfg = (0x34 / 4),            /* MAIN Domain Boot Configuration Register */
	ctrlmmr_reg_main_feature_stat0 = (0x40 / 4),      /* MAIN Domain Feature Status Register 0 */
	ctrlmmr_reg_main_feature_stat1 = (0x44 / 4),      /* MAIN Domain Feature Status Register 1 */
	ctrlmmr_reg_ipc_set0 = (0x100 / 4),               /* IPC Generation Register 0 */
	ctrlmmr_reg_ipc_set6 = (0x118 / 4),               /* IPC Generation Register 6 */
	ctrlmmr_reg_ipc_set7 = (0x11c / 4),               /* IPC Generation Register 7 */
	ctrlmmr_reg_ipc_set8 = (0x120 / 4),               /* IPC Generation Register 8 */
	ctrlmmr_reg_ipc_set9 = (0x124 / 4),               /* IPC Generation Register 9 */
	ctrlmmr_reg_ipc_set16 = (0x140 / 4),              /* IPC Generation Register 16 */
	ctrlmmr_reg_ipc_set17 = (0x144 / 4),              /* IPC Generation Register 17 */
	ctrlmmr_reg_ipc_set18 = (0x148 / 4),              /* IPC Generation Register 18 */
	ctrlmmr_reg_ipc_set19 = (0x14c / 4),              /* IPC Generation Register 19 */
	ctrlmmr_reg_ipc_set20 = (0x150 / 4),              /* IPC Generation Register 20 */
	ctrlmmr_reg_ipc_set21 = (0x154 / 4),              /* IPC Generation Register 21 */
	ctrlmmr_reg_ipc_set22 = (0x158 / 4),              /* IPC Generation Register 22 */
	ctrlmmr_reg_ipc_set23 = (0x15c / 4),              /* IPC Generation Register 23 */
	ctrlmmr_reg_ipc_clr0 = (0x180 / 4),               /* IPC Acknowledge Register0 */
	ctrlmmr_reg_ipc_clr6 = (0x198 / 4),               /* IPC Acknowledge Register6 */
	ctrlmmr_reg_ipc_clr7 = (0x19c / 4),               /* IPC Acknowledge Register7 */
	ctrlmmr_reg_ipc_clr8 = (0x1a0 / 4),               /* IPC Acknowledge Register8 */
	ctrlmmr_reg_ipc_clr9 = (0x1a4 / 4),               /* IPC Acknowledge Register9 */
	ctrlmmr_reg_ipc_clr16 = (0x1c0 / 4),              /* IPC Acknowledge Register 16 */
	ctrlmmr_reg_ipc_clr17 = (0x1c4 / 4),              /* IPC Acknowledge Register 17 */
	ctrlmmr_reg_ipc_clr18 = (0x1c8 / 4),              /* IPC Acknowledge Register 18 */
	ctrlmmr_reg_ipc_clr19 = (0x1cc / 4),              /* IPC Acknowledge Register 19 */
	ctrlmmr_reg_ipc_clr20 = (0x1d0 / 4),              /* IPC Acknowledge Register 20 */
	ctrlmmr_reg_ipc_clr21 = (0x1d4 / 4),              /* IPC Acknowledge Register 21 */
	ctrlmmr_reg_ipc_clr22 = (0x1d8 / 4),              /* IPC Acknowledge Register 22 */
	ctrlmmr_reg_ipc_clr23 = (0x1dc / 4),              /* IPC Acknowledge Register 23 */
	ctrlmmr_reg_pci_device_id = (0x210 / 4),          /* PCI Device ID Register */
	ctrlmmr_reg_usb_device_id = (0x220 / 4),          /* USB Device ID Register */
	ctrlmmr_reg_lock0_kick0 = (0x1008 / 4),           /* Partition 0 Lock Key 0 Register */
	ctrlmmr_reg_lock0_kick1 = (0x100c / 4),           /* Partition 0 Lock Key 1 Register */
	ctrlmmr_reg_intr_raw_stat = (0x1010 / 4),         /* Interrupt Raw Status Register */
	ctrlmmr_reg_intr_stat_clr = (0x1014 / 4),         /* Interrupt Status and Clear Register */
	ctrlmmr_reg_intr_en_set = (0x1018 / 4),           /* Interrupt Enable Set Register */
	ctrlmmr_reg_intr_en_clr = (0x101c / 4),           /* Interrupt Enable Clear Register */
	ctrlmmr_reg_eoi = (0x1020 / 4),                   /* End of Interrupt Register */
	ctrlmmr_reg_fault_addr = (0x1024 / 4),            /* Fault Address Register */
	ctrlmmr_reg_fault_type = (0x1028 / 4),            /* Fault Type Register */
	ctrlmmr_reg_fault_attr = (0x102c / 4),            /* Fault Attribute Register */
	ctrlmmr_reg_fault_clr = (0x1030 / 4),             /* Fault Clear Register */
	ctrlmmr_reg_p0_claim0 = (0x1100 / 4),             /* Partition 0 Claim Register 0 */
	ctrlmmr_reg_p0_claim1 = (0x1104 / 4),             /* Partition 0 Claim Register 1 */
	ctrlmmr_reg_p0_claim2 = (0x1108 / 4),             /* Partition 0 Claim Register 2 */
	ctrlmmr_reg_p0_claim3 = (0x110c / 4),             /* Partition 0 Claim Register 3 */
	ctrlmmr_reg_p0_claim4 = (0x1110 / 4),             /* Partition 0 Claim Register 4 0010 1110h */
	ctrlmmr_reg_p0_claim5 = (0x1114 / 4),             /* Partition 0 Claim Register 5 0010 1114h */
	ctrlmmr_reg_p0_claim6 = (0x1118 / 4),             /* Partition 0 Claim Register 6 0010 1118h */
	ctrlmmr_reg_usb0_ctrl = (0x4000 / 4),             /* USB0 Control Register */
	ctrlmmr_reg_usb1_ctrl = (0x4010 / 4),             /* USB1 Control Register */
	ctrlmmr_reg_enet1_ctrl = (0x4044 / 4),            /* Ethernet1 Control Register */
	ctrlmmr_reg_enet2_ctrl = (0x4048 / 4),            /* Ethernet2 Control Register */
	ctrlmmr_reg_enet3_ctrl = (0x404c / 4),            /* Ethernet3 Control Register */
	ctrlmmr_reg_enet4_ctrl = (0x4050 / 4),            /* Ethernet4 Control Register */
	ctrlmmr_reg_enet5_ctrl = (0x4054 / 4),            /* Ethernet5 Control Register */
	ctrlmmr_reg_enet6_ctrl = (0x4058 / 4),            /* Ethernet6 Control Register */
	ctrlmmr_reg_enet7_ctrl = (0x405c / 4),            /* Ethernet7 Control Register */
	ctrlmmr_reg_enet8_ctrl = (0x4060 / 4),            /* Ethernet8 Control Register */
	ctrlmmr_reg_pcie0_ctrl = (0x4070 / 4),            /* PCEI0 Control Register */
	ctrlmmr_reg_pcie1_ctrl = (0x4074 / 4),            /* PCEI1 Control Register */
	ctrlmmr_reg_pcie2_ctrl = (0x4078 / 4),            /* PCEI2 Control Register */
	ctrlmmr_reg_pcie3_ctrl = (0x407c / 4),            /* PCEI3 Control Register */
	ctrlmmr_reg_serdes0_ln0_ctrl = (0x4080 / 4),      /* SERDES0 Lane0 Control Register */
	ctrlmmr_reg_serdes0_ln1_ctrl = (0x4084 / 4),      /* SERDES0 Lane1 Control Register */
	ctrlmmr_reg_serdes1_ln0_ctrl = (0x4090 / 4),      /* SERDES1 Lane0 Control Register */
	ctrlmmr_reg_serdes1_ln1_ctrl = (0x4094 / 4),      /* SERDES1 Lane1 Control Register */
	ctrlmmr_reg_serdes2_ln0_ctrl = (0x40a0 / 4),      /* SERDES2 Lane0 Control Register */
	ctrlmmr_reg_serdes2_ln1_ctrl = (0x40a4 / 4),      /* SERDES2 Lane1 Control Register */
	ctrlmmr_reg_serdes3_ln0_ctrl = (0x40b0 / 4),      /* SERDES3 Lane0 Control Register */
	ctrlmmr_reg_serdes3_ln1_ctrl = (0x40b4 / 4),      /* SERDES3 Lane1 Control Register */
	ctrlmmr_reg_serdes4_ln0_ctrl = (0x40c0 / 4),      /* SERDES4 Lane0 Control Register */
	ctrlmmr_reg_serdes4_ln1_ctrl = (0x40c4 / 4),      /* SERDES4 Lane1 Control Register */
	ctrlmmr_reg_serdes4_ln2_ctrl = (0x40c8 / 4),      /* SERDES4 Lane2 Control Register */
	ctrlmmr_reg_serdes4_ln3_ctrl = (0x40cc / 4),      /* SERDES4 Lane3 Control Register */
	ctrlmmr_reg_serdes0_ctrl = (0x40e0 / 4),          /* SERDES0 Control Register */
	ctrlmmr_reg_serdes1_ctrl = (0x40e4 / 4),          /* SERDES1 Control Register */
	ctrlmmr_reg_serdes2_ctrl = (0x40e8 / 4),          /* SERDES2 Control Register */
	ctrlmmr_reg_serdes3_ctrl = (0x40ec / 4),          /* SERDES3 Control Register */
	ctrlmmr_reg_serdes4_ctrl = (0x40f0 / 4),          /* SERDES4 Control Register */
	ctrlmmr_reg_icssg0_ctrl0 = (0x4100 / 4),          /* ICSS_G0 Control Register 0 */
	ctrlmmr_reg_icssg0_ctrl1 = (0x4104 / 4),          /* ICSS_G0 Control Register 1 */
	ctrlmmr_reg_icssg1_ctrl0 = (0x4110 / 4),          /* ICSS_G1 Control Register 0 */
	ctrlmmr_reg_icssg1_ctrl1 = (0x4114 / 4),          /* ICSS_G1 Control Register 1 */
	ctrlmmr_reg_epwm0_ctrl = (0x4140 / 4),            /* PWM0 Control Register */
	ctrlmmr_reg_epwm1_ctrl = (0x4144 / 4),            /* PWM1 Control Register */
	ctrlmmr_reg_epwm2_ctrl = (0x4148 / 4),            /* PWM2 Control Register */
	ctrlmmr_reg_epwm3_ctrl = (0x414c / 4),            /* PWM3 Control Register */
	ctrlmmr_reg_epwm4_ctrl = (0x4150 / 4),            /* PWM4 Control Register */
	ctrlmmr_reg_epwm5_ctrl = (0x4154 / 4),            /* PWM5 Control Register */
	ctrlmmr_reg_soca_sel = (0x4160 / 4),              /* PWM SOCA Select Register */
	ctrlmmr_reg_socb_sel = (0x4164 / 4),              /* PWM SOCB Select Register */
	ctrlmmr_reg_eqep_stat = (0x41a0 / 4),             /* EQEP Status Register */
	ctrlmmr_reg_sdio1_ctrl = (0x41b4 / 4),            /* SDIO1 Control Register */
	ctrlmmr_reg_sdio2_ctrl = (0x41b8 / 4),            /* SDIO2 Control Register */
	ctrlmmr_reg_mlb_sig_io_ctrl = (0x41c0 / 4),       /* MLB SIG LVDS IO Control Register */
	ctrlmmr_reg_mlb_dat_io_ctrl = (0x41c4 / 4),       /* MLB DATA LVDS IO Control Register */
	ctrlmmr_reg_mlb_clk_io_ctrl = (0x41c8 / 4),       /* MLB CLK LVDS IO Control Register */
	ctrlmmr_reg_mlb_gpio_ctrl = (0x41d0 / 4),         /* MLB GPIO Control Register */
	ctrlmmr_reg_timer0_ctrl = (0x4200 / 4),           /* TIMER0 Control Register */
	ctrlmmr_reg_timer1_ctrl = (0x4204 / 4),           /* TIMER1 Control Register */
	ctrlmmr_reg_timer2_ctrl = (0x4208 / 4),           /* TIMER2 Control Register */
	ctrlmmr_reg_timer3_ctrl = (0x420c / 4),           /* TIMER3 Control Register */
	ctrlmmr_reg_timer4_ctrl = (0x4210 / 4),           /* TIMER4 Control Register */
	ctrlmmr_reg_timer5_ctrl = (0x4214 / 4),           /* TIMER5 Control Register */
	ctrlmmr_reg_timer6_ctrl = (0x4218 / 4),           /* TIMER6 Control Register */
	ctrlmmr_reg_timer7_ctrl = (0x421c / 4),           /* TIMER7 Control Register */
	ctrlmmr_reg_timer8_ctrl = (0x4220 / 4),           /* TIMER8 Control Register */
	ctrlmmr_reg_timer9_ctrl = (0x4224 / 4),           /* TIMER9 Control Register */
	ctrlmmr_reg_timer10_ctrl = (0x4228 / 4),          /* TIMER10 Control Register */
	ctrlmmr_reg_timer11_ctrl = (0x422c / 4),          /* TIMER11 Control Register */
	ctrlmmr_reg_timer12_ctrl = (0x4230 / 4),          /* TIMER12 Control Register */
	ctrlmmr_reg_timer13_ctrl = (0x4234 / 4),          /* TIMER13 Control Register */
	ctrlmmr_reg_timer14_ctrl = (0x4238 / 4),          /* TIMER14 Control Register */
	ctrlmmr_reg_timer15_ctrl = (0x423c / 4),          /* TIMER15 Control Register */
	ctrlmmr_reg_timer16_ctrl = (0x4240 / 4),          /* TIMER16 Control Register */
	ctrlmmr_reg_timer17_ctrl = (0x4244 / 4),          /* TIMER17 Control Register */
	ctrlmmr_reg_timer18_ctrl = (0x4248 / 4),          /* TIMER18 Control Register */
	ctrlmmr_reg_timer19_ctrl = (0x424c / 4),          /* TIMER19 Control Register */
	ctrlmmr_reg_timerio0_ctrl = (0x4280 / 4),         /* TIMERIO0 Control Register */
	ctrlmmr_reg_timerio1_ctrl = (0x4284 / 4),         /* TIMERIO1 Control Register */
	ctrlmmr_reg_timerio2_ctrl = (0x4288 / 4),         /* TIMERIO2 Control Register */
	ctrlmmr_reg_timerio3_ctrl = (0x428c / 4),         /* TIMERIO3 Control Register */
	ctrlmmr_reg_timerio4_ctrl = (0x4290 / 4),         /* TIMERIO4 Control Register */
	ctrlmmr_reg_timerio5_ctrl = (0x4294 / 4),         /* TIMERIO5 Control Register */
	ctrlmmr_reg_timerio6_ctrl = (0x4298 / 4),         /* TIMERIO6 Control Register */
	ctrlmmr_reg_timerio7_ctrl = (0x429c / 4),         /* TIMERIO7 Control Register */
	ctrlmmr_reg_i3c0_ctrl0 = (0x42c0 / 4),            /* I3C0 Control Register 0 */
	ctrlmmr_reg_i3c0_ctrl1 = (0x42c4 / 4),            /* I3C0 Control Register 1 */
	ctrlmmr_reg_i3c1_ctrl0 = (0x42c8 / 4),            /* I3C1 Control Register 0 */
	ctrlmmr_reg_i3c1_ctrl1 = (0x42cc / 4),            /* I3C1 Control Register 1 */
	ctrlmmr_reg_i2c0_ctrl = (0x42e0 / 4),             /* I2C0 Control Register */
	ctrlmmr_reg_i2c1_ctrl = (0x42e4 / 4),             /* I2C1 Control Register */
	ctrlmmr_reg_csi_rx_loopback = (0x43f0 / 4),       /* CSI-RX Loopback Control Register */
	ctrlmmr_reg_gpu_gp_in_req = (0x4500 / 4),         /* GPU GPIO In Request Register */
	ctrlmmr_reg_gpu_gp_in_ack = (0x4504 / 4),         /* GPU GPIO In Acknowledge Register */
	ctrlmmr_reg_gpu_gp_out_req = (0x4508 / 4),        /* GPU GPIO Out Request Register */
	ctrlmmr_reg_gpu_gp_out_ack = (0x450c / 4),        /* GPU GPIO Out Acknowledge Register */
	ctrlmmr_reg_ufs_phy_cal_ctrl2 = (0x4548 / 4),     /* UFS PHY Calibration Control Register 2 */
	ctrlmmr_reg_lock1_kick0 = (0x5008 / 4),           /* Partition 1 Lock Key 0 Register */
	ctrlmmr_reg_lock1_kick1 = (0x500c / 4),           /* Partition 1 Lock Key 1 Register */
	ctrlmmr_reg_p1_claim0 = (0x5100 / 4),             /* Partition 1 Claim Register 0 */
	ctrlmmr_reg_p1_claim1 = (0x5104 / 4),             /* Partition 1 Claim Register 1 */
	ctrlmmr_reg_p1_claim2 = (0x5108 / 4),             /* Partition 1 Claim Register 2 */
	ctrlmmr_reg_p1_claim3 = (0x510c / 4),             /* Partition 1 Claim Register 3 */
	ctrlmmr_reg_p1_claim4 = (0x5110 / 4),             /* Partition 1 Claim Register 4 */
	ctrlmmr_reg_p1_claim5 = (0x5114 / 4),             /* Partition 1 Claim Register 5 */
	ctrlmmr_reg_p1_claim6 = (0x5118 / 4),             /* Partition 1 Claim Register 6 */
	ctrlmmr_reg_p1_claim7 = (0x511c / 4),             /* Partition 1 Claim Register 7 */
	ctrlmmr_reg_p1_claim8 = (0x5120 / 4),             /* Partition 1 Claim Register 8 */
	ctrlmmr_reg_p1_claim9 = (0x5124 / 4),             /* Partition 1 Claim Register 9 */
	ctrlmmr_reg_p1_claim10 = (0x5128 / 4),            /* Partition 1 Claim Register 10 */
	ctrlmmr_reg_p1_claim11 = (0x512c / 4),            /* Partition 1 Claim Register 11 */
	ctrlmmr_reg_p1_claim12 = (0x5130 / 4),            /* Partition 1 Claim Register 12 */
	ctrlmmr_reg_p1_claim13 = (0x5134 / 4),            /* Partition 1 Claim Register 13 */
	ctrlmmr_reg_p1_claim14 = (0x5138 / 4),            /* Partition 1 Claim Register 14 */
	ctrlmmr_reg_p1_claim15 = (0x513c / 4),            /* Partition 1 Claim Register 15 */
	ctrlmmr_reg_p1_claim16 = (0x5140 / 4),            /* Partition 1 Claim Register 16 */
	ctrlmmr_reg_p1_claim17 = (0x5144 / 4),            /* Partition 1 Claim Register 17 */
	ctrlmmr_reg_p1_claim18 = (0x5148 / 4),            /* Partition 1 Claim Register 18 */
	ctrlmmr_reg_p1_claim19 = (0x514c / 4),            /* Partition 1 Claim Register 19 */
	ctrlmmr_reg_p1_claim20 = (0x5150 / 4),            /* Partition 1 Claim Register 20 */
	ctrlmmr_reg_obsclk0_ctrl = (0x8000 / 4),          /* Observe Clock 0 Output Control Register */
	ctrlmmr_reg_obsclk1_ctrl = (0x8004 / 4),          /* Observe Clock 1 Select Register */
	ctrlmmr_reg_clkout_ctrl = (0x8010 / 4),           /* CLKOUT Control Register */
	ctrlmmr_reg_gtc_clksel = (0x8030 / 4),            /* GTC Clock Select Register */
	ctrlmmr_reg_efuse_clksel = (0x803c / 4),          /* Main eFuse Controller Clock Select Register */
	ctrlmmr_reg_icssg0_clksel = (0x8040 / 4),         /* ICSS_G0 Clock Select Register */
	ctrlmmr_reg_icssg1_clksel = (0x8044 / 4),         /* ICSS_G1 Clock Select Register */
	ctrlmmr_reg_pcie0_clksel = (0x8080 / 4),          /* PCIE0 Clock Select Register */
	ctrlmmr_reg_pcie1_clksel = (0x8084 / 4),          /* PCIE1 Clock Select Register */
	ctrlmmr_reg_pcie2_clksel = (0x8088 / 4),          /* PCIE2 Clock Select Register */
	ctrlmmr_reg_pcie3_clksel = (0x808c / 4),          /* PCIE3 Clock Select Register */
	ctrlmmr_reg_cpsw_clksel = (0x8090 / 4),           /* CPSW Clock Select Register */
	ctrlmmr_reg_navss_clksel = (0x8098 / 4),          /* Navigator Subsystem Clock Select Register */
	ctrlmmr_reg_emmc0_clksel = (0x80b0 / 4),          /* eMMC0 Clock Select Register */
	ctrlmmr_reg_emmc1_clksel = (0x80b4 / 4),          /* eMMC1 Clock Select Register */
	ctrlmmr_reg_emmc2_clksel = (0x80b8 / 4),          /* eMMC2 Clock Select Register */
	ctrlmmr_reg_ufs0_clksel = (0x80c0 / 4),           /* UFS0 Clock Select Register */
	ctrlmmr_reg_gpmc_clksel = (0x80d0 / 4),           /* GPMC Clock Select Register */
	ctrlmmr_reg_usb0_clksel = (0x80e0 / 4),           /* USB0 Clock Select Register */
	ctrlmmr_reg_usb1_clksel = (0x80e4 / 4),           /* USB1 Clock Select Register */
	ctrlmmr_reg_timer0_clksel = (0x8100 / 4),         /* Timer0 Clock Select Register */
	ctrlmmr_reg_timer1_clksel = (0x8104 / 4),         /* Timer1 Clock Select Register */
	ctrlmmr_reg_timer2_clksel = (0x8108 / 4),         /* Timer2 Clock Select Register */
	ctrlmmr_reg_timer3_clksel = (0x810c / 4),         /* Timer3 Clock Select Register */
	ctrlmmr_reg_timer4_clksel = (0x8110 / 4),         /* Timer4 Clock Select Register */
	ctrlmmr_reg_timer5_clksel = (0x8114 / 4),         /* Timer5 Clock Select Register */
	ctrlmmr_reg_timer6_clksel = (0x8118 / 4),         /* Timer6 Clock Select Register */
	ctrlmmr_reg_timer7_clksel = (0x811c / 4),         /* Timer7 Clock Select Register */
	ctrlmmr_reg_timer8_clksel = (0x8120 / 4),         /* Timer8 Clock Select Register */
	ctrlmmr_reg_timer9_clksel = (0x8124 / 4),         /* Timer9 Clock Select Register */
	ctrlmmr_reg_timer10_clksel = (0x8128 / 4),        /* Timer10 Clock Select Register */
	ctrlmmr_reg_timer11_clksel = (0x812c / 4),        /* Timer11 Clock Select Register */
	ctrlmmr_reg_timer12_clksel = (0x8130 / 4),        /* Timer12 Clock Select Register */
	ctrlmmr_reg_timer13_clksel = (0x8134 / 4),        /* Timer13 Clock Select Register */
	ctrlmmr_reg_timer14_clksel = (0x8138 / 4),        /* Timer14 Clock Select Register */
	ctrlmmr_reg_timer15_clksel = (0x813c / 4),        /* Timer15 Clock Select Register */
	ctrlmmr_reg_timer16_clksel = (0x8140 / 4),        /* Timer16 Clock Select Register */
	ctrlmmr_reg_timer17_clksel = (0x8144 / 4),        /* Timer17 Clock Select Register */
	ctrlmmr_reg_timer18_clksel = (0x8148 / 4),        /* Timer18 Clock Select Register */
	ctrlmmr_reg_timer19_clksel = (0x814c / 4),        /* Timer19 Clock Select Register */
	ctrlmmr_reg_spi0_clksel = (0x8190 / 4),           /* SPI0 Clock Select Register */
	ctrlmmr_reg_spi1_clksel = (0x8194 / 4),           /* SPI1 Clock Select Register */
	ctrlmmr_reg_spi2_clksel = (0x8198 / 4),           /* SPI2 Clock Select Register */
	ctrlmmr_reg_spi3_clksel = (0x819c / 4),           /* SPI3 Clock Select Register */
	ctrlmmr_reg_spi5_clksel = (0x81a4 / 4),           /* SPI5 Clock Select Register */
	ctrlmmr_reg_spi6_clksel = (0x81a8 / 4),           /* SPI6 Clock Select Register */
	ctrlmmr_reg_spi7_clksel = (0x81ac / 4),           /* SPI7 Clock Select Register */
	ctrlmmr_reg_usart0_clk_ctrl = (0x81c0 / 4),       /* USART0 Functional Clock Control */
	ctrlmmr_reg_usart1_clk_ctrl = (0x81c4 / 4),       /* USART1 Functional Clock Control */
	ctrlmmr_reg_usart2_clk_ctrl = (0x81c8 / 4),       /* USART2 Functional Clock Control */
	ctrlmmr_reg_usart3_clk_ctrl = (0x81cc / 4),       /* USART3 Functional Clock Control */
	ctrlmmr_reg_usart4_clk_ctrl = (0x81d0 / 4),       /* USART4 Functional Clock Control */
	ctrlmmr_reg_usart5_clk_ctrl = (0x81d4 / 4),       /* USART5 Functional Clock Control */
	ctrlmmr_reg_usart6_clk_ctrl = (0x81d8 / 4),       /* USART6 Functional Clock Control */
	ctrlmmr_reg_usart7_clk_ctrl = (0x81dc / 4),       /* USART7 Functional Clock Control */
	ctrlmmr_reg_usart8_clk_ctrl = (0x81e0 / 4),       /* USART8 Functional Clock Control */
	ctrlmmr_reg_usart9_clk_ctrl = (0x81e4 / 4),       /* USART9 Functional Clock Control */
	ctrlmmr_reg_mcasp0_clksel = (0x8200 / 4),         /* McASP0 Clock Select Register */
	ctrlmmr_reg_mcasp1_clksel = (0x8204 / 4),         /* McASP1 Clock Select Register */
	ctrlmmr_reg_mcasp2_clksel = (0x8208 / 4),         /* McASP2 Clock Select Register */
	ctrlmmr_reg_mcasp3_clksel = (0x820c / 4),         /* McASP3 Clock Select Register */
	ctrlmmr_reg_mcasp4_clksel = (0x8210 / 4),         /* McASP4 Clock Select Register */
	ctrlmmr_reg_mcasp5_clksel = (0x8214 / 4),         /* McASP5 Clock Select Register */
	ctrlmmr_reg_mcasp6_clksel = (0x8218 / 4),         /* McASP6 Clock Select Register */
	ctrlmmr_reg_mcasp7_clksel = (0x821c / 4),         /* McASP7 Clock Select Register */
	ctrlmmr_reg_mcasp8_clksel = (0x8220 / 4),         /* McASP8 Clock Select Register */
	ctrlmmr_reg_mcasp9_clksel = (0x8224 / 4),         /* McASP9 Clock Select Register */
	ctrlmmr_reg_mcasp10_clksel = (0x8228 / 4),        /* McASP10 Clock Select Register */
	ctrlmmr_reg_mcasp11_clksel = (0x822c / 4),        /* McASP11 Clock Select Register */
	ctrlmmr_reg_mcasp0_ahclksel = (0x8240 / 4),       /* McASP0 AHClock Select Register */
	ctrlmmr_reg_mcasp1_ahclksel = (0x8244 / 4),       /* McASP1 AHClock Select Register */
	ctrlmmr_reg_mcasp2_ahclksel = (0x8248 / 4),       /* McASP2 AHClock Select Register */
	ctrlmmr_reg_mcasp3_ahclksel = (0x824c / 4),       /* McASP3 AHClock Select Register */
	ctrlmmr_reg_mcasp4_ahclksel = (0x8250 / 4),       /* McASP4 AHClock Select Register */
	ctrlmmr_reg_mcasp5_ahclksel = (0x8254 / 4),       /* McASP5 AHClock Select Register */
	ctrlmmr_reg_mcasp6_ahclksel = (0x8258 / 4),       /* McASP6 AHClock Select Register */
	ctrlmmr_reg_mcasp7_ahclksel = (0x825c / 4),       /* McASP7 AHClock Select Register */
	ctrlmmr_reg_mcasp8_ahclksel = (0x8260 / 4),       /* McASP8 AHClock Select Register */
	ctrlmmr_reg_mcasp9_ahclksel = (0x8264 / 4),       /* McASP9 AHClock Select Register */
	ctrlmmr_reg_mcasp10_ahclksel = (0x8268 / 4),      /* McASP10 AHClock Select Register */
	ctrlmmr_reg_mcasp11_ahclksel = (0x826c / 4),      /* McASP11 AHClock Select Register */
	ctrlmmr_reg_asrc_rxsync0_sel = (0x8280 / 4),      /* ASRC Receive Frame Sync Select Register */
	ctrlmmr_reg_asrc_rxsync1_sel = (0x8284 / 4),      /* ASRC Receive Frame Sync Select Register */
	ctrlmmr_reg_asrc_rxsync2_sel = (0x8288 / 4),      /* ASRC Receive Frame Sync Select Register */
	ctrlmmr_reg_asrc_rxsync3_sel = (0x828c / 4),      /* ASRC Receive Frame Sync Select Register */
	ctrlmmr_reg_asrc_txsync0_sel = (0x8290 / 4),      /* ASRC Transmit Frame Sync Select Register */
	ctrlmmr_reg_asrc_txsync1_sel = (0x8294 / 4),      /* ASRC Transmit Frame Sync Select Register */
	ctrlmmr_reg_asrc_txsync2_sel = (0x8298 / 4),      /* ASRC Transmit Frame Sync Select Register */
	ctrlmmr_reg_asrc_txsync3_sel = (0x829c / 4),      /* ASRC Transmit Frame Sync Select Register */
	ctrlmmr_reg_atl_bws0_sel = (0x82a0 / 4),          /* ATL BWS0 Select Register */
	ctrlmmr_reg_atl_bws1_sel = (0x82a4 / 4),          /* ATL BWS1 Select Register */
	ctrlmmr_reg_atl_bws2_sel = (0x82a8 / 4),          /* ATL BWS2 Select Register */
	ctrlmmr_reg_atl_bws3_sel = (0x82ac / 4),          /* ATL BWS3 Select Register */
	ctrlmmr_reg_atl_aws0_sel = (0x82b0 / 4),          /* ATL AWS Select Register */
	ctrlmmr_reg_atl_aws1_sel = (0x82b4 / 4),          /* ATL AWS Select Register */
	ctrlmmr_reg_atl_aws2_sel = (0x82b8 / 4),          /* ATL AWS Select Register */
	ctrlmmr_reg_atl_aws3_sel = (0x82bc / 4),          /* ATL AWS Select Register */
	ctrlmmr_reg_atl_clksel = (0x82c0 / 4),            /* ATL Clock Select Register */
	ctrlmmr_reg_dpi0_clk_ctrl = (0x8300 / 4),         /* DPI0 Clock Control Register */
	ctrlmmr_reg_dpi1_clk_ctrl = (0x8304 / 4),         /* DPI1 Clock Control Register */
	ctrlmmr_reg_dphy0_clksel = (0x8310 / 4),          /* DSI0 PHY Clock Select Register */
	ctrlmmr_reg_edp_phy0_clksel = (0x8340 / 4),       /* EDP Phy0 Clock Select Register */
	ctrlmmr_reg_edp0_clk_ctrl = (0x8350 / 4),         /* EDP0 Clock Control Register */
	ctrlmmr_reg_wwd0_clksel = (0x8380 / 4),           /* WWD0 Clock Select Register */
	ctrlmmr_reg_wwd1_clksel = (0x8384 / 4),           /* WWD1 Clock Select Register */
	ctrlmmr_reg_wwd15_clksel = (0x83bc / 4),          /* WWD15 Clock Select Register */
	ctrlmmr_reg_wwd16_clksel = (0x83c0 / 4),          /* WWD16 Clock Select Register */
	ctrlmmr_reg_wwd24_clksel = (0x83e0 / 4),          /* WWD24 Clock Select Register */
	ctrlmmr_reg_wwd25_clksel = (0x83e4 / 4),          /* WWD25 Clock Select Register */
	ctrlmmr_reg_wwd28_clksel = (0x83f0 / 4),          /* WWD28 Clock Select Register */
	ctrlmmr_reg_wwd29_clksel = (0x83f4 / 4),          /* WWD29 Clock Select Register */
	ctrlmmr_reg_wwd30_clksel = (0x83f8 / 4),          /* WWD30 Clock Select Register */
	ctrlmmr_reg_wwd31_clksel = (0x83fc / 4),          /* WWD31 Clock Select Register */
	ctrlmmr_reg_serdes0_clksel = (0x8400 / 4),        /* SERDES 0 Clock Select Register */
	ctrlmmr_reg_serdes0_clk1sel = (0x8404 / 4),       /* SERDES 0 Clock1 Select Register */
	ctrlmmr_reg_serdes1_clksel = (0x8410 / 4),        /* SERDES 1 Clock Select Register */
	ctrlmmr_reg_serdes1_clk1sel = (0x8414 / 4),       /* SERDES 1 Clock1 Select Register */
	ctrlmmr_reg_serdes2_clksel = (0x8420 / 4),        /* SERDES 2 Clock Select Register */
	ctrlmmr_reg_serdes2_clk1sel = (0x8424 / 4),       /* SERDES 2 Clock1 Select Register */
	ctrlmmr_reg_serdes3_clksel = (0x8430 / 4),        /* SERDES 3 Clock Select Register */
	ctrlmmr_reg_serdes3_clk1sel = (0x8434 / 4),       /* SERDES 3 Clock1 Select Register */
	ctrlmmr_reg_mcan0_clksel = (0x8480 / 4),          /* MCAN0 Clock Select Register */
	ctrlmmr_reg_mcan1_clksel = (0x8484 / 4),          /* MCAN1 Clock Select Register */
	ctrlmmr_reg_mcan2_clksel = (0x8488 / 4),          /* MCAN2 Clock Select Register */
	ctrlmmr_reg_mcan3_clksel = (0x848c / 4),          /* MCAN3 Clock Select Register */
	ctrlmmr_reg_mcan4_clksel = (0x8490 / 4),          /* MCAN4 Clock Select Register */
	ctrlmmr_reg_mcan5_clksel = (0x8494 / 4),          /* MCAN5 Clock Select Register */
	ctrlmmr_reg_mcan6_clksel = (0x8498 / 4),          /* MCAN6 Clock Select Register */
	ctrlmmr_reg_mcan7_clksel = (0x849c / 4),          /* MCAN7 Clock Select Register */
	ctrlmmr_reg_mcan8_clksel = (0x84a0 / 4),          /* MCAN8 Clock Select Register */
	ctrlmmr_reg_mcan9_clksel = (0x84a4 / 4),          /* MCAN9 Clock Select Register */
	ctrlmmr_reg_mcan10_clksel = (0x84a8 / 4),         /* MCAN10 Clock Select Register */
	ctrlmmr_reg_mcan11_clksel = (0x84ac / 4),         /* MCAN11 Clock Select Register */
	ctrlmmr_reg_mcan12_clksel = (0x84b0 / 4),         /* MCAN12 Clock Select Register */
	ctrlmmr_reg_mcan13_clksel = (0x84b4 / 4),         /* MCAN13 Clock Select Register */
	ctrlmmr_reg_lock2_kick0 = (0x9008 / 4),           /* Partition 2 Lock Key 0 Register */
	ctrlmmr_reg_lock2_kick1 = (0x900c / 4),           /* Partition 2 Lock Key 1 Register */
	ctrlmmr_reg_p2_claim0 = (0x9100 / 4),             /* Partition 2 Claim Register 0 */
	ctrlmmr_reg_p2_claim1 = (0x9104 / 4),             /* Partition 2 Claim Register 1 */
	ctrlmmr_reg_p2_claim2 = (0x9108 / 4),             /* Partition 2 Claim Register 2 */
	ctrlmmr_reg_p2_claim3 = (0x910c / 4),             /* Partition 2 Claim Register 3 */
	ctrlmmr_reg_p2_claim4 = (0x9110 / 4),             /* Partition 2 Claim Register 4 */
	ctrlmmr_reg_p2_claim5 = (0x9114 / 4),             /* Partition 2 Claim Register 5 */
	ctrlmmr_reg_p2_claim6 = (0x9118 / 4),             /* Partition 2 Claim Register 6 */
	ctrlmmr_reg_p2_claim7 = (0x911c / 4),             /* Partition 2 Claim Register 7 */
	ctrlmmr_reg_p2_claim8 = (0x9120 / 4),             /* Partition 2 Claim Register 8 */
	ctrlmmr_reg_p2_claim9 = (0x9124 / 4),             /* Partition 2 Claim Register 9 */
	ctrlmmr_reg_mcu0_lbist_ctrl = (0xc000 / 4),       /* SoC_Pulsar Logic BIST Control Register */
	ctrlmmr_reg_mcu0_lbist_patcount = (0xc004 / 4),   /* SoC_Pulsar Logic BIST Pattern Count Register */
	ctrlmmr_reg_mcu0_lbist_seed0 = (0xc008 / 4),      /* SoC_Pulsar Logic BIST Seed0 Register */
	ctrlmmr_reg_mcu0_lbist_seed1 = (0xc00c / 4),      /* SoC_Pulsar Logic BIST Seed1 Register */
	ctrlmmr_reg_mcu0_lbist_spare0 = (0xc010 / 4),     /* SoC_Pulsar Logic BIST Spare0 Register */
	ctrlmmr_reg_mcu0_lbist_spare1 = (0xc014 / 4),     /* SoC_Pulsar Logic BIST Spare1 Register */
	ctrlmmr_reg_mcu0_lbist_stat = (0xc018 / 4),       /* SoC_Pulsar Logic BIST Status Register */
	ctrlmmr_reg_mcu0_lbist_misr = (0xc01c / 4),       /* SoC_Pulsar Logic BIST MISR Register */
	ctrlmmr_reg_mcu1_lbist_ctrl = (0xc020 / 4),       /* SoC_Pulsar Logic BIST Control Register */
	ctrlmmr_reg_mcu1_lbist_patcount = (0xc024 / 4),   /* SoC_Pulsar Logic BIST Pattern Count Register */
	ctrlmmr_reg_mcu1_lbist_seed0 = (0xc028 / 4),      /* SoC_Pulsar Logic BIST Seed0 Register */
	ctrlmmr_reg_mcu1_lbist_seed1 = (0xc02c / 4),      /* SoC_Pulsar Logic BIST Seed1 Register */
	ctrlmmr_reg_mcu1_lbist_spare0 = (0xc030 / 4),     /* SoC_Pulsar Logic BIST Spare0 Register */
	ctrlmmr_reg_mcu1_lbist_spare1 = (0xc034 / 4),     /* SoC_Pulsar Logic BIST Spare1 Register */
	ctrlmmr_reg_mcu1_lbist_stat = (0xc038 / 4),       /* SoC_Pulsar Logic BIST Status Register */
	ctrlmmr_reg_mcu1_lbist_misr = (0xc03c / 4),       /* SoC_Pulsar Logic BIST MISR Register */
	ctrlmmr_reg_dmpac_lbist_ctrl = (0xc040 / 4),      /* DMPAC Logic BIST Control Register */
	ctrlmmr_reg_dmpac_lbist_seed0 = (0xc048 / 4),     /* DMPAC Logic BIST Seed0 Register */
	ctrlmmr_reg_dmpac_lbist_seed1 = (0xc04c / 4),     /* DMPAC Logic BIST Seed1 Register */
	ctrlmmr_reg_dmpac_lbist_spare0 = (0xc050 / 4),    /* DMPAC Logic BIST Spare0 Register */
	ctrlmmr_reg_dmpac_lbist_spare1 = (0xc054 / 4),    /* DMPAC Logic BIST Spare1 Register */
	ctrlmmr_reg_dmpac_lbist_stat = (0xc058 / 4),      /* DMPAC Logic BIST Status Register */
	ctrlmmr_reg_dmpac_lbist_misr = (0xc05c / 4),      /* DMPAC Logic BIST MISR Register */
	ctrlmmr_reg_vpac_lbist_ctrl = (0xc060 / 4),       /* VPAC Logic BIST Control Register */
	ctrlmmr_reg_vpac_lbist_seed0 = (0xc068 / 4),      /* VPAC Logic BIST Seed0 Register */
	ctrlmmr_reg_vpac_lbist_seed1 = (0xc06c / 4),      /* VPAC Logic BIST Seed1 Register */
	ctrlmmr_reg_vpac_lbist_spare0 = (0xc070 / 4),     /* VPAC Logic BIST Spare0 Register */
	ctrlmmr_reg_vpac_lbist_spare1 = (0xc074 / 4),     /* VPAC Logic BIST Spare1 Register */
	ctrlmmr_reg_vpac_lbist_stat = (0xc078 / 4),       /* VPAC Logic BIST Status Register */
	ctrlmmr_reg_vpac_lbist_misr = (0xc07c / 4),       /* VPAC Logic BIST MISR Register */
	ctrlmmr_reg_dsp0_lbist_ctrl = (0xc080 / 4),       /* DSP Cluster0 Logic BIST Control Register */
	ctrlmmr_reg_dsp0_lbist_seed0 = (0xc088 / 4),      /* DSP Cluster0 Logic BIST Seed0 Register */
	ctrlmmr_reg_dsp0_lbist_seed1 = (0xc08c / 4),      /* DSP Cluster0 Logic BIST Seed1 Register */
	ctrlmmr_reg_dsp0_lbist_spare0 = (0xc090 / 4),     /* DSP Cluster0 Logic BIST Spare0 Register */
	ctrlmmr_reg_dsp0_lbist_spare1 = (0xc094 / 4),     /* DSP Cluster0 Logic BIST Spare1 Register */
	ctrlmmr_reg_dsp0_lbist_stat = (0xc098 / 4),       /* DSP Cluster0 Logic BIST Status Register */
	ctrlmmr_reg_dsp0_lbist_misr = (0xc09c / 4),       /* DSP Cluster0 Logic BIST MISR Register */
	ctrlmmr_reg_mpu0_lbist_ctrl = (0xc100 / 4),       /* ARM Cluster0 Logic BIST Control Register */
	ctrlmmr_reg_mpu0_lbist_seed0 = (0xc108 / 4),      /* ARM Cluster0 Logic BIST Seed0 Register */
	ctrlmmr_reg_mpu0_lbist_seed1 = (0xc10c / 4),      /* ARM Cluster0 Logic BIST Seed1 Register */
	ctrlmmr_reg_mpu0_lbist_spare0 = (0xc110 / 4),     /* ARM Cluster0 Logic BIST Spare0 Register */
	ctrlmmr_reg_mpu0_lbist_spare1 = (0xc114 / 4),     /* ARM Cluster0 Logic BIST Spare1 Register */
	ctrlmmr_reg_mpu0_lbist_stat = (0xc118 / 4),       /* ARM Cluster0 Logic BIST Status Register */
	ctrlmmr_reg_mpu0_lbist_misr = (0xc11c / 4),       /* ARM Cluster0 Logic BIST MISR Register */
	ctrlmmr_reg_dmpac_lbist_sig = (0xc288 / 4),       /* DMPAC Logic BIST MISR Signature Register */
	ctrlmmr_reg_vpac_lbist_sig = (0xc28c / 4),        /* VPAC Logic BIST MISR Signature Register */
	ctrlmmr_reg_dsp0_lbist_sig = (0xc290 / 4),        /* DSP0 Logic BIST MISR Signature Register */
	ctrlmmr_reg_fuse_crc_stat = (0xc320 / 4),         /* MAIN eFUse CRC Status Register */
	ctrlmmr_reg_lock3_kick0 = (0xd008 / 4),           /* Partition 3 Lock Key 0 Register */
	ctrlmmr_reg_lock3_kick1 = (0xd00c / 4),           /* Partition 3 Lock Key 1 Register */
	ctrlmmr_reg_p3_claim0 = (0xd100 / 4),             /* Partition 3 Claim Register 0 */
	ctrlmmr_reg_p3_claim1 = (0xd104 / 4),             /* Partition 3 Claim Register 1 */
	ctrlmmr_reg_p3_claim2 = (0xd108 / 4),             /* Partition 3 Claim Register 2 */
	ctrlmmr_reg_p3_claim3 = (0xd10c / 4),             /* Partition 3 Claim Register 3 */
	ctrlmmr_reg_p3_claim4 = (0xd110 / 4),             /* Partition 3 Claim Register 4 */
	ctrlmmr_reg_p3_claim5 = (0xd114 / 4),             /* Partition 3 Claim Register 5 */
	ctrlmmr_reg_p3_claim6 = (0xd118 / 4),             /* Partition 3 Claim Register 6 */
	ctrlmmr_reg_chng_ddr4_fsp_req = (0x14000 / 4),    /* Change LPDDR4 FSP Request Register */
	ctrlmmr_reg_chng_ddr4_fsp_ack = (0x14004 / 4),    /* Change LPDDR4 FSP Acknowledge Register */
	ctrlmmr_reg_ddr4_fsp_clkchng_req = (0x14080 / 4), /* LPDDR4 FSP Clock Change Request Register */
	ctrlmmr_reg_ddr4_fsp_clkchng_ack = (0x140c0 / 4), /* LPDDR4 FSP Clock Change Acknowledge Register */
	ctrlmmr_reg_lock5_kick0 = (0x15008 / 4),          /* Partition 5 Lock Key 0 Register */
	ctrlmmr_reg_lock5_kick1 = (0x1500c / 4),          /* Partition 5 Lock Key 1 Register */
	ctrlmmr_reg_p5_claim0 = (0x15100 / 4),            /* Partition 5 Claim Register 0 */
	ctrlmmr_reg_p5_claim1 = (0x15104 / 4),            /* Partition 5 Claim Register 1 */
	ctrlmmr_reg_acspcie0_ctrl = (0x18090 / 4),        /* ACSPCIE 0 Control Register */
	ctrlmmr_reg_acspcie1_ctrl = (0x18094 / 4),        /* ACSPCIE 1 Control Register */
	ctrlmmr_reg_lock6_kick0 = (0x19008 / 4),          /* Partition 6 Lock Key 0 Register */
	ctrlmmr_reg_lock6_kick1 = (0x1900c / 4),          /* Partition 6 Lock Key 1 Register */
	ctrlmmr_reg_p6_claim0 = (0x19100 / 4),            /* Partition 6 Claim Register 0 */
	ctrlmmr_reg_p6_claim1 = (0x19104 / 4),            /* Partition 6 Claim Register 1 */
	ctrlmmr_reg_padconfig0 = (0x1c000 / 4),           /* PAD Configuration Register 0 */
	ctrlmmr_reg_lock7_kick0 = (0x1d008 / 4),          /* Partition 7 Lock Key 0 Register */
	ctrlmmr_reg_lock7_kick1 = (0x1d00c / 4),          /* Partition 7 Lock Key 1 Register */
	ctrlmmr_reg_p7_claim0 = (0x1d100 / 4),            /* Partition 7 Claim Register 0 */
	ctrlmmr_reg_p7_claim1 = (0x1d104 / 4),            /* Partition 7 Claim Register 1 */
	ctrlmmr_reg_p7_claim2 = (0x1d108 / 4),            /* Partition 7 Claim Register 2 */
	ctrlmmr_reg_p7_claim3 = (0x1d10c / 4),            /* Partition 7 Claim Register 3 */
	ctrlmmr_reg_p7_claim4 = (0x1d110 / 4),            /* Partition 7 Claim Register 4 */
	ctrlmmr_reg_p7_claim5 = (0x1d114 / 4),            /* Partition 7 Claim Register 5 */
	ctrlmmr_reg_dphy_tx0_ctrl = (0x4300 / 4),         /* Video Transmit MIPI DPHY0 Lane0 Control Register */
	ctrlmmr_reg_pcie_refclk0_clksel = (0x8070 / 4),   /* PCIE Reference Clock 0 Select Register */
	ctrlmmr_reg_pcie_refclk1_clksel = (0x8074 / 4),   /* PCIE Reference Clock 1 Select Register */
	ctrlmmr_reg_pcie_refclk2_clksel = (0x8078 / 4),   /* PCIE Reference Clock 2 Select Register */
	ctrlmmr_reg_pcie_refclk3_clksel = (0x807c / 4),   /* PCIE Reference Clock 3 Select Register */
	ctrlmmr_reg_audio_refclk0_ctrl = (0x82e0 / 4),    /* Audio External Reference Clock Control Register */
	ctrlmmr_reg_audio_refclk1_ctrl = (0x82e4 / 4),    /* Audio External Reference Clock Control Register */
	ctrlmmr_reg_audio_refclk2_ctrl = (0x82e8 / 4),    /* Audio External Reference Clock Control Register */
	ctrlmmr_reg_audio_refclk3_ctrl = (0x82ec / 4),    /* Audio External Reference Clock Control Register */
	ctrlmmr_reg_dss_dispc0_clksel1 = (0x8324 / 4),    /* DSS Display Controller0 Clock Select1 Register */
	ctrlmmr_reg_dss_dispc0_clksel2 = (0x8328 / 4),    /* DSS Display Controller0 Clock Select2 Register */
	ctrlmmr_reg_dss_dispc0_clksel3 = (0x832c / 4),    /* DSS Display Controller0 Clock Select3 Register */
	ctrlmmr_reg_dmpac_lbist_patcount = (0xc044 / 4),  /* DMPAC Logic BIST Pattern Count Register */
	ctrlmmr_reg_vpac_lbist_patcount = (0xc064 / 4),   /* VPAC Logic BIST Pattern Count Register */
	ctrlmmr_reg_dsp0_lbist_patcount = (0xc084 / 4),   /* DSP Cluster0 Logic BIST Pattern Count Register */
	ctrlmmr_reg_mpu0_lbist_patcount = (0xc104 / 4),   /* ARM Cluster0 Logic BIST Pattern Count Register */
	ctrlmmr_reg_mcu0_lbist_sig = (0xc280 / 4),        /* MCU Cluster0 Logic BIST MISR Signature Register */
	ctrlmmr_reg_mcu1_lbist_sig = (0xc284 / 4),        /* MCU Cluster1 Logic BIST MISR Signature Register */
	ctrlmmr_reg_mpu0_lbist_sig = (0xc2a0 / 4),        /* ARM Cluster0 Logic BIST MISR Signature Register */
};

#endif /* _TDA4VM_REG_DEFS_H_ */
