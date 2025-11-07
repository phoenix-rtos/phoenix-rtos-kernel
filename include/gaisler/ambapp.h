/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * GRLIB AMBA Plug&Play definitions
 *
 * Copyright 2023, 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PH_GAISLER_AMBAPP_H_
#define _PH_GAISLER_AMBAPP_H_

/* GRLIB Cores' IDs - pages 10-18 GRLIB IP CORE Manual
 * https://www.gaisler.com/products/grlib/grip.pdf#page=10
 */

/* Processor license functions */
#define CORE_ID_LEON3    0x003U
#define CORE_ID_LEON3FT  0x053U
#define CORE_ID_DSU3     0x004U
#define CORE_ID_L3STAT   0x098U
#define CORE_ID_LEON4    0x048U
#define CORE_ID_LEON4FT  0x048U
#define CORE_ID_L4STAT   0x047U
#define CORE_ID_DSU4     0x049U
#define CORE_ID_LEON5    0x0baU
#define CORE_ID_LEON5FT  0x0baU
#define CORE_ID_DSU5     0x0bbU
#define CORE_ID_NOEL_V   0x0bdU
#define CORE_ID_NOEL_VFT 0x0bdU
#define CORE_ID_RVDM     0x0beU
#define CORE_ID_L2CACHE  0x04bU
#define CORE_ID_L2C_LITE 0x0d0U
#define CORE_ID_GRIOMMU  0x04fU
#define CORE_ID_GRIOMMU2 0x0d3U

/* Processor support functions */
#define CORE_ID_GPTIMER    0x011U
#define CORE_ID_GRCLKGATE  0x02cU
#define CORE_ID_GRDMAC     0x095U
#define CORE_ID_GRDMAC2    0x0c0U
#define CORE_ID_GRTIMER    0x038U
#define CORE_ID_GRWATCHDOG 0x0c9U
#define CORE_ID_IRQMP      0x00dU
#define CORE_ID_IRQAMP     0x00dU

/* Memory controllers and supporting cores */
#define CORE_ID_DDRSPA     0x025U
#define CORE_ID_DDR2SPA    0x02eU
#define CORE_ID_MCTRL      0x00fU
#define CORE_ID_SDCTRL     0x009U
#define CORE_ID_SRCTRL     0x008U
#define CORE_ID_SSRCTRL    0x00aU
#define CORE_ID_FTADDR     0x0aeU
#define CORE_ID_FTMCTRL    0x054U
#define CORE_ID_FTSDCTRL   0x055U
#define CORE_ID_FTSDCTRL64 0x058U
#define CORE_ID_FTSRCTRL   0x051U
#define CORE_ID_FTSRCTRL8  0x056U
#define CORE_ID_NANDFCTRL  0x059U
#define CORE_ID_NANDFCTRL2 0x0c5U
#define CORE_ID_SPIMCTRL   0x045U
#define CORE_ID_AHBSTAT    0x052U
#define CORE_ID_MEMSCRUB   0x057U

/* AMBA Bus control */
#define CORE_ID_AHB2AHB   0x020U
#define CORE_ID_AHB2AVLA  0x096U
#define CORE_ID_AHB2AXIB  0x09fU
#define CORE_ID_AHBBRIDGE 0x020U
#define CORE_ID_APBCTRL   0x006U
#define CORE_ID_AHBTRACE  0x017U
#define CORE_ID_MMA       0x07fU

/* PCI interface */
#define CORE_ID_GRPCI2       0x07cU
#define CORE_ID_PCITARGET    0x012U
#define CORE_ID_PCIMTF_GRPCI 0x014U
#define CORE_ID_PCITRACE     0x015U
#define CORE_ID_PCIDMA       0x016U
#define CORE_ID_PCIARB       0x010U

/* On-chip memory functions */
#define CORE_ID_AHBRAM   0x00eU
#define CORE_ID_AHBDPRAM 0x00fU
#define CORE_ID_AHBROM   0x01bU
#define CORE_ID_FTAHBRAM 0x050U

/* Serial communication */
#define CORE_ID_AHBUART     0x007U
#define CORE_ID_AHBJTAG     0x01cU
#define CORE_ID_APBPS2      0x060U
#define CORE_ID_APBUART     0x00cU
#define CORE_ID_CAN_OC      0x019U
#define CORE_ID_CANMUX      0x081U
#define CORE_ID_GRCAN       0x03dU
#define CORE_ID_GRCANFD     0x0b5U
#define CORE_ID_GRHSSL      0x0c8U
#define CORE_ID_GRSPFI      0x0bcU
#define CORE_ID_GRSPW2      0x029U
#define CORE_ID_GRSPW2_DMA  0x08aU
#define CORE_ID_GRSPWROUTER 0x08bU
#define CORE_ID_GRSPWTDP    0x097U
#define CORE_ID_GRSRIO      0x0a8U
#define CORE_ID_GRWIZL      0x0c7U
#define CORE_ID_I2C2AHB     0x00bU
#define CORE_ID_I2CMST      0x028U
#define CORE_ID_I2CSLV      0x03eU
#define CORE_ID_SOCBRIDGE   0x0c4U
#define CORE_ID_SPI2AHB     0x05cU
#define CORE_ID_SPICTRL     0x02dU
#define CORE_ID_SPIMASTER   0x0a6U
#define CORE_ID_SPISLAVE    0x0a7U

/* Ethernet interface */
#define CORE_ID_GRETH      0x01dU
#define CORE_ID_GRETH_GBIT 0x01dU
#define CORE_ID_RGMII      0x093U

/* USB interface */
#define CORE_ID_GRUSBHC 0x027U
#define CORE_ID_GRUSBDC 0x022U

/* MIL-STD-1553B interface */
#define CORE_ID_GR1553B    0x04dU
#define CORE_ID_GRB1553BRM 0x072U

/* Encryption and compression */
#define CORE_ID_GRAES     0x073U
#define CORE_ID_GRAES_DMA 0x07bU
#define CORE_ID_GRECC     0x074U
#define CORE_ID_GRSHYLOC  0x0b7U

/* FPGA control and error mitigation */
#define CORE_ID_GRSCRUB 0x0c1U

/* Graphics functions */
#define CORE_ID_APBVGA   0x061U
#define CORE_ID_SVGACTRL 0x063U

/* Auxiliary functions */
#define CORE_ID_GRACECTRL 0x067U
#define CORE_ID_GRADCDAC  0x036U
#define CORE_ID_GRFIFO    0x035U
#define CORE_ID_GRGPIO    0x01aU
#define CORE_ID_GRGPREG   0x087U
#define CORE_ID_GRGPRBANK 0x08fU
#define CORE_ID_GRPULSE   0x037U
#define CORE_ID_GRPWM     0x04aU
#define CORE_ID_GRSYSMON  0x066U
#define CORE_ID_GRVERSION 0x03aU

/* Spacecraft data handling functions */
#define CORE_ID_GRTM      0x030U
#define CORE_ID_GRTM_DESC 0x084U
#define CORE_ID_GRTM_VC   0x085U
#define CORE_ID_GRTM_PAHB 0x088U
#define CORE_ID_GRGEFFE   0x086U
#define CORE_ID_GRTMRX    0x082U
#define CORE_ID_GRTC      0x031U
#define CORE_ID_GRTCTX    0x083U
#define CORE_ID_GRCTM     0x033U
#define CORE_ID_SPWCUC    0x089U
#define CORE_ID_GRPW      0x032U
#define CORE_ID_GRPWRX    0x08eU
#define CORE_ID_GRPWTX    0x08dU
#define CORE_ID_APB2PW    0x03bU
#define CORE_ID_PW2APB    0x03cU
#define CORE_ID_AHB2PP    0x039U
#define CORE_ID_GRRM      0x09aU

#define AMBA_TYPE_APBIO  0x1U
#define AMBA_TYPE_AHBMEM 0x2U
#define AMBA_TYPE_AHBIO  0x3U

#define BUS_AMBA_AHB 0x0U
#define BUS_AMBA_APB 0x1U


typedef struct _ambapp_dev_t {
	unsigned short devId;
	unsigned char vendor;
	unsigned char irqn;
	unsigned int bus;
	union {
		struct {
			unsigned int *base[4];
			unsigned int type[4];
		} ahb;
		struct {
			unsigned int *base;
			unsigned int type;
		} apb;
	} info;
} __attribute__((packed)) ambapp_dev_t;


#endif
