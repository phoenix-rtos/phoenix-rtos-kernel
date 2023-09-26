/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration detection and other initialisation routines
 *
 * Copyright 2023 Phoenix Systems
 * Author: Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ACPI_H
#define _HAL_ACPI_H

#include <arch/types.h>
#include "config.h"
#include "arch/pmap.h"

#define MADT_TYPE_PROCESSOR_LOCAL_APIC             0
#define MADT_TYPE_IOAPIC                           1
#define MADT_TYPE_IOAPIC_INTERRUPT_SOURCE_OVERRIDE 2

#define MADT_8259PIC_INSTALLED (1u << 0)

/* IAPC_BOOT_ARCH flags in FADT: acpi->fadt->iapcBootArch */
#define FADT_LEGACY_DEVICES      (1u << 0)
#define FADT_KEYBOARD_CONTROLLER (1u << 1)
#define FADT_NO_VGA              (1u << 2)
#define FADT_MSI_NOT_SUPPORTED   (1u << 3)
#define FADT_PCIe_ASPM_CONTROLS  (1u << 4)
#define FADT_NO_CMOS_RTC         (1u << 5)

#define HAL_MEM_ENTRIES        64
#define MMIO_DEVICES_VIRT_ADDR (void *)0xfe000000

typedef struct {
	char magic[4];
	u32 length;
	u8 revision;
	u8 checksum;
	char oemId[6];
	char oemiTableId[8];
	u32 oemRevision;
	u32 creatorId;
	u32 creatorRevision;
} __attribute__((packed)) sdt_header_t;


typedef struct {
	sdt_header_t header;
	addr_t sdt[];
} __attribute__((packed)) rsdt_t;


typedef struct {
	char magic[8];
	u8 checksum;
	char oemId[6];
	u8 revision;
	addr_t rsdt;
} __attribute__((packed)) rsdp_t;


typedef struct {
	rsdp_t rsdp;

	u32 length;
	u64 xsdtAddr;
	u8 extChecksum;
	u8 _reserved[3];
} __attribute__ ((packed)) xsdp_t;


typedef struct {
	sdt_header_t header;
	addr_t localApicAddr;
	u32 flags;
	u8 entries[]; /* It is an array of variable length elements */
} __attribute__ ((packed)) madt_header_t;

typedef struct {
	u8 addressSpaceId;
	u8 registerWidth;
	u8 registerOffset;
	u8 accessSize;
	u64 address;
} __attribute__ ((packed)) generic_address_structure_t;

typedef struct {
	sdt_header_t header;
	u32 firmwareCtrl;
	u32 dsdt; /* DSDT */
	u8 reserved;
	u8 preferredPmProfile; /* Preferred_PM_Profile */
	u16 sciInt;            /* SCI_INT */
	u32 smiCmd;            /* SMI_CMD */
	u8 acpiEnable;         /* ACPI_ENABLE */
	u8 acpiDisable;        /* ACPI_DISABLE */
	u8 s4BiosReq;          /* S4BIOS_REQ */
	u8 pstateCnt;          /* PSTATE_CNT */
	u32 pm1aEvtBlk;        /* PM1a_EVT_BLK */
	u32 pm1bEvtBlk;        /* PM1b_EVT_BLK */
	u32 pm1aCntBlk;        /* PM1a_CNT_BLK */
	u32 pm1bCntBlk;        /* PM1b_CNT_BLK */
	u32 pm2CntBlk;         /* PM2_CNT_BLK */
	u32 pmTmrBlk;          /* PM_TMR_BLK */
	u32 gpe0Blk;           /* GPE0_BLK */
	u32 gpe1Blk;           /* GPE1_BLK */
	u8 pm1EvtLen;          /* PM1_EVT_LEN */
	u8 pm1CntLen;          /* PM1_CNT_LEN */
	u8 pm2CntLen;          /* PM2_CNT_LEN */
	u8 pmTmrLen;           /* PM_TMR_LEN */
	u8 gpe0BlkLen;         /* GPE0_BLK_LEN */
	u8 gpe1BlkLen;         /* GPE1_BLK_LEN */
	u8 gpe1Base;           /* GPE1_BASE */
	u8 cstCnt;             /* CST_CNT */
	u16 pLvl2Lat;          /* P_LVL2_LAT */
	u16 pLvl3Lat;          /* P_LVL3_LAT */
	u16 flushSize;         /* FLUSH_SIZE */
	u16 flushStride;       /* FLUSH_STRIDE */
	u8 dutyOffset;         /* DUTY_OFFSET */
	u8 dutyWidth;          /* DUTY_WIDTH */
	u8 dayAlarm;           /* DAY_ALRM */
	u8 monAlarm;           /* MON_ALRM */
	u8 century;            /* CENTURY */
	u16 iapcBootArch;      /* IAPC_BOOT_ARCH */
	u8 reserved2;
	u32 flags;
	generic_address_structure_t resetReg; /* RESET_REG */
	u8 resetValue;                        /* RESET_VALUE */
	u16 armBootArch;                      /* ARM_BOOT_ARCH */
	u8 fadtMinorVersion;
	u64 xFirmwareCtrl;                           /* X_FIRMWARE_CTRL */
	u64 xDsdt;                                   /* X_DSDT */
	generic_address_structure_t xPm1aEvtBlk;     /* X_PM1a_EVT_BLK */
	generic_address_structure_t xPm1bEvtBlk;     /* X_PM1b_EVT_BLK */
	generic_address_structure_t xPm1aCntBlk;     /* X_PM1a_CNT_BLK */
	generic_address_structure_t xPm1bCntBlk;     /* X_PM1b_CNT_BLK */
	generic_address_structure_t xPm2CntBlk;      /* X_PM2_CNT_BLK */
	generic_address_structure_t xPmTmrBlk;       /* X_PM_TMR_BLK */
	generic_address_structure_t xGpe0Blk;        /* X_GPE0_BLK */
	generic_address_structure_t xGpe1Blk;        /* X_GPE1_BLK */
	generic_address_structure_t sleepControlReg; /* SLEEP_CONTROL_REG */
	generic_address_structure_t sleepStatusReg;  /* SLEEP_STATUS_REG */
	u64 hypervisorVendorIdentity;

} __attribute__ ((packed)) fadt_header_t;


typedef struct {
	addr_t start;
	u32 pageCount;
	u32 flags;
} hal_memEntry_t;


typedef struct {
	void *localApicAddr;
	unsigned int acpi;
	addr_t ebda;
	u32 flags;
	addr_t minAddr;
	addr_t maxAddr;
	void *heapStart;
	addr_t *ptable;
	madt_header_t *madt;
	fadt_header_t *fadt;
	void *devices; /* Address space, where memory mapped devices go */
	struct {
		u32 count;
		hal_memEntry_t entries[HAL_MEM_ENTRIES];
	} memMap;
} hal_config_t;


extern hal_config_t hal_config;


static inline int hal_isLapicPresent(void)
{
	return hal_config.localApicAddr != NULL;
}


void _hal_configInit(syspage_t *s);


void *_hal_configMapDevice(u32 *pdir, addr_t start, size_t size, int attr);


#endif
