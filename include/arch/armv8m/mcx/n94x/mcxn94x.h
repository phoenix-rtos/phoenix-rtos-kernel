/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Macros and enums for MCXN94x related code
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_MCXN94X_H_
#define _PHOENIX_ARCH_MCXN94X_H_


#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */

/* IRQ numbers */
enum {
	all_irq = 16, edma0_ch0_irq, edma0_ch1_irq, edma0_ch2_irq, edma0_ch3_irq, edma0_ch4_irq,
	edma0_ch5_irq, edma0_ch6_irq, edma0_ch7_irq, edma0_ch8_irq, edma0_ch9_irq, edma0_ch10_irq,
	edma0_ch11_irq, edma0_ch12_irq, edma0_ch13_irq, edma0_ch14_irq, edma0_ch15_irq, gpio0_0_irq,
	gpio0_1_irq, gpio1_0_irq, gpio1_1_irq, gpio2_0_irq, gpio2_1_irq, gpio3_0_irq, gpio3_1_irq,
	gpio4_0_irq, gpio4_1_irq, gpio5_0_irq, gpio5_1_irq, utick0_irq, mrt0_irq, ctimer0_irq, ctimer1_irq,
	sct0_irq, ctimer2_irq, lp_flexcomm0_irq, lp_flexcomm1_irq, lp_flexcomm2_irq, lp_flexcomm3_irq,
	lp_flexcomm4_irq, lp_flexcomm5_irq, lp_flexcomm6_irq, lp_flexcomm7_irq, lp_flexcomm8_irq,
	lp_flexcomm9_irq, adc0_irq, adc1_irq, pint0_irq, micfil0_irq, /* reserved */ usbfs0_irq = 16 + 50,
	usbdcd0_irq, rtc0_irq, smartdma_irq, mailbox0_irq, ctimer3_irq, ctimer4_irq, ostimer0_irq, flexspi0_irq,
	sai0_irq, sai1_irq, usdhc0_irq, can0_irq, can1_irq, /* 2 reserved */ usbhs1_phy_irq = 16 + 66, usbhs1_irq,
	/* secure irq */ /* reserved */ plu0_irq = 16 + 70, freqme0_irq, /* 4 secure irq */ powerquad0_irq = 16 + 76,
	edma1_ch0_irq, edma1_ch1_irq, edma1_ch2_irq, edma1_ch3_irq, edma1_ch4_irq, edma1_ch5_irq, edma1_ch6_irq,
	edma1_ch7_irq, edma1_ch8_irq, edma1_ch9_irq, edma1_ch10_irq, edma1_ch11_irq, edma1_ch12_irq, edma1_ch13_irq,
	edma1_ch14_irq, edma1_ch15_irq, /* 2 secure irq */ i3c0_irq = 16 + 95, i3c1_irq, npu_irq, /* secure irq */
	vbat0_irq = 16 + 99, ewm0_irq, tsi0_irq, tsi1_irq, emvsim0_irq, emvsim1_irq, flexio0_irq, dac0_irq, dac1_irq,
	dac2_irq, cmp0_irq, cmp1_irq, cmp2_irq, pwm0re_irq, pwm0f_irq, pwm0ccr0_irq, pwm0ccr1_irq, pwm0ccr2_irq,
	pwm0ccr3_irq, pwm1re_irq, pwm1f_irq, pwm1ccr0_irq, pwm1ccr1_irq, pwm1ccr2_irq, pwm1ccr3_irq, enc0c_irq,
	enc0h_irq, enc0wdg_irq, enc0idx_irq, enc1c_irq, enc1h_irq, enc1wdg_irq, enc1idx_irq, /* secure irq */
	bsp32_irq = 16 + 133, /* 2 secure irq */ erm0s_irq = 16 + 136, erm0m_irq, fmu0_irq, enet0_irq, enet0pm_irq,
	enet0lp1_irq, sinc0_irq, lpmtr0_irq, lptmr1_irq, scg0_irq, spc0_irq, wuu0_irq, port_irq, etb0_irq,
	/* 2 reserved */ wwdt0_irq = 16 + 152, wwdt1_irq, cmc0_irq, ct_irq
};

/* Pinout */
enum {
	/* Port 0 */
	pctl_pin_p0_0 = 0, pctl_pin_p0_1, pctl_pin_p0_2, pctl_pin_p0_3, pctl_pin_p0_4, pctl_pin_p0_5,
	pctl_pin_p0_6, pctl_pin_p0_7, pctl_pin_p0_8, pctl_pin_p0_9, pctl_pin_p0_10, pctl_pin_p0_11,
	pctl_pin_p0_12, pctl_pin_p0_13, pctl_pin_p0_14, pctl_pin_p0_15, pctl_pin_p0_16, pctl_pin_p0_17,
	pctl_pin_p0_18, pctl_pin_p0_19, pctl_pin_p0_20, pctl_pin_p0_21, pctl_pin_p0_22, pctl_pin_p0_23,
	pctl_pin_p0_24, pctl_pin_p0_25, pctl_pin_p0_26, pctl_pin_p0_27, pctl_pin_p0_28, pctl_pin_p0_29,
	pctl_pin_p0_30, pctl_pin_p0_31,

	/* Port 1 */
	pctl_pin_p1_0 = 32, pctl_pin_p1_1, pctl_pin_p1_2, pctl_pin_p1_3, pctl_pin_p1_4, pctl_pin_p1_5,
	pctl_pin_p1_6, pctl_pin_p1_7 ,pctl_pin_p1_8, pctl_pin_p1_9, pctl_pin_p1_10, pctl_pin_p1_11,
	pctl_pin_p1_12, pctl_pin_p1_13, pctl_pin_p1_14, pctl_pin_p1_15, pctl_pin_p1_16, pctl_pin_p1_17,
	pctl_pin_p1_18, pctl_pin_p1_19, pctl_pin_p1_20, pctl_pin_p1_21, pctl_pin_p1_22, pctl_pin_p1_23,
	/* pins 24-29 missing */ pctl_pin_p1_30 = 32 + 30, pctl_pin_p1_31,

	/* Port 2 */
	pctl_pin_p2_0 = 64, pctl_pin_p2_1, pctl_pin_p2_2, pctl_pin_p2_3, pctl_pin_p2_4, pctl_pin_p2_5,
	pctl_pin_p2_6, pctl_pin_p2_7, pctl_pin_p2_8, pctl_pin_p2_9, pctl_pin_p2_10, pctl_pin_p2_11,
	/* pins 12-31 missing */

	/* Port 3 */
	pctl_pin_p3_0 = 96, pctl_pin_p3_1, pctl_pin_p3_2, pctl_pin_p3_3, pctl_pin_p3_4, pctl_pin_p3_5,
	pctl_pin_p3_6, pctl_pin_p3_7, pctl_pin_p3_8, pctl_pin_p3_9, pctl_pin_p3_10, pctl_pin_p3_11,
	pctl_pin_p3_12, pctl_pin_p3_13, pctl_pin_p3_14, pctl_pin_p3_15, pctl_pin_p3_16, pctl_pin_p3_17,
	pctl_pin_p3_18, pctl_pin_p3_19, pctl_pin_p3_20, pctl_pin_p3_21, pctl_pin_p3_22, pctl_pin_p3_23,
	/* pins 24-31 missing */

	/* Port 4 */
	pctl_pin_p4_0 = 128, pctl_pin_p4_1, pctl_pin_p4_2, pctl_pin_p4_3, pctl_pin_p4_4, pctl_pin_p4_5,
	pctl_pin_p4_6, pctl_pin_p4_7, /* pins 8-11 missing */ pctl_pin_p4_12 = 128 + 12, pctl_pin_p4_13,
	pctl_pin_p4_14, pctl_pin_p4_15, pctl_pin_p4_16, pctl_pin_p4_17, pctl_pin_p4_18, pctl_pin_p4_19,
	pctl_pin_p4_20, pctl_pin_p4_21, pctl_pin_p4_22, pctl_pin_p4_23, /* pins 24-31 missing */

	/* Port 5 */
	pctl_pin_p5_0 = 160, pctl_pin_p5_1, pctl_pin_p5_2, pctl_pin_p5_3, pctl_pin_p5_4, pctl_pin_p5_5,
	pctl_pin_p5_6, pctl_pin_p5_7, pctl_pin_p5_8, pctl_pin_p5_9, /* pins 10-31 missing */
};


/* Pin options */
#define MCX_PIN_INVERT (1 << 13)
#define MCX_PIN_INPUT_BUFFER_ENABLE (1 << 12)
#define MCX_PIN_SLOW (1 << 3)
#define MCX_PIN_FAST 0
#define MCX_PIN_STRONG (1 << 6)
#define MCX_PIN_WEAK 0
#define MCX_PIN_OPEN_DRAIN (1 << 5)
#define MCX_PIN_FILTER_ENABLE (1 << 4)
#define MCX_PIN_PULLDOWN_WEAK ((1 << 1) | (1 << 2))
#define MCX_PIN_PULLDOWN_STRONG (1 << 1)
#define MCX_PIN_PULLUP_WEAK ((1 << 0) | (1 << 1) | (1 << 2))
#define MCX_PIN_PULLUP_STRONG ((1 << 0) | (1 << 1))


/* Peripherals */
enum {
	/* AHB0 */
	/* reserved */ pctl_rom = 1, pctl_ramb, pctl_ramc, pctl_ramd, pctl_rame, pctl_ramf, pctl_ramg,
	pctl_ramh, pctl_fmu, pctl_fmc, pctl_flexspi, pctl_mux, pctl_port0, pctl_port1, pctl_port2,
	pctl_port3, pctl_port4, /* reserved */ pctl_gpio0 = 19, pctl_gpio1, pctl_gpio2, pctl_gpio3,
	pctl_gpio4, /* reserved */ pctl_pint = 25, pctl_dma0, pctl_crc, pctl_wwdt0, pctl_wwdt1,
	/* reserved */ pctl_mailbox = 31,

	/* AHB1 */
	pctl_mrt = 32, pctl_ostimer, pctl_sct, pctl_adc0, pctl_adc1, pctl_dac0, pctl_rtc, /* reserved */
	pctl_evsim0 = 32 + 8, pctl_evsim1, pctl_utick, pctl_fc0, pctl_fc1, pctl_fc2, pctl_fc3, pctl_fc4,
	pctl_fc5, pctl_fc6, pctl_fc7, pctl_fc8, pctl_fc9, pctl_micfil, pctl_timer2, /* reserved */
	pctl_usb0fsdcd = 32 + 24, pctl_usb0fs, pctl_timer0, pctl_timer1, /* 2 reserved */
	pctl_smartdma = 32 + 31,

	/* AHB2 */
	/* reserved */ pctl_dma1 = 64 + 1, pctl_enet, pctl_usdhc, pctl_flexio, pctl_sai0, pctl_sai1,
	/* reserved */ pctl_freqme = 64 + 8, /* 5 reserved */ pctl_flexcan0 = 64 + 14, pctl_flexcan1,
	pctl_usbhs, pctl_usbhsphy, /* reserved */ pctl_pq = 64 + 19, pctl_plulut, pctl_timer3, pctl_timer4,
	/* 3 reserved */ pctl_scq = 64 + 26, /* 5 reserved */

	/* AHB3 */
	pctl_i3c0 = 96, pctl_i3c1, pctl_sinc, pctl_coolflux, pctl_enc0, pctl_enc1, pctl_pwm0, pctl_pwm1,
	pctl_evtg, /* 2 reserved */ pctl_dac1 = 96 + 11, pctl_dac2, pctl_opamp0, pctl_opamp1, pctl_opamp2,
	pctl_cmp0, pctl_cmp1, pctl_cmp2, pctl_vref, pctl_coolfluxapb, pctl_npu, pctl_tsi, pctl_ewm,
	pctl_eim, pctl_erm, pctl_intm, pctl_sema42, /* 4 reserved */

	/* Misc clocks */
	pctl_cmp0rr = 128, pctl_cmp1rr, pctl_cmp2rr,
	pctl_i3c0s, pctl_i3c1s, pctl_i3c0stc, pctl_i3c1stc
};
/* clang-format off */


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_devclk = 0, pctl_devrst, pctl_pinConf, pctl_reboot } type;

	union {
		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;

		struct {
			int dev;
			unsigned int sel;
			unsigned int div;
			int enable;
		} devClk;

		struct {
			int dev;
			int state;
		} devRst;

		struct {
			int pin;
			int mux;
			int options;
		} pinConf;
	};
} __attribute__((packed)) platformctl_t;


#endif
