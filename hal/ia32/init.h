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

#ifndef _PH_HAL_INIT_H_
#define _PH_HAL_INIT_H_

#include "hal/types.h"
#include "config.h"
#include "arch/pmap.h"

#define MADT_TYPE_PROCESSOR_LOCAL_APIC             0U
#define MADT_TYPE_IOAPIC                           1U
#define MADT_TYPE_IOAPIC_INTERRUPT_SOURCE_OVERRIDE 2U

#define MADT_8259PIC_INSTALLED (1U << 0)

/* IAPC_BOOT_ARCH flags in FADT: acpi->fadt->iapcBootArch */
#define FADT_LEGACY_DEVICES      (1U << 0)
#define FADT_KEYBOARD_CONTROLLER (1U << 1)
#define FADT_NO_VGA              (1U << 2)
#define FADT_MSI_NOT_SUPPORTED   (1U << 3)
#define FADT_PCIe_ASPM_CONTROLS  (1U << 4)
#define FADT_NO_CMOS_RTC         (1U << 5)

#define HAL_MEM_ENTRIES        64U
#define MMIO_DEVICES_VIRT_ADDR (void *)0xfe000000U

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


#define GAS_ADDRESS_SPACE_ID_MEMORY 0x0U
#define GAS_ADDRESS_SPACE_ID_IOPORT 0x1U
#define GAS_ADDRESS_SPACE_ID_PCI    0x2U
#define GAS_ADDRESS_SPACE_ID_EMBEDD 0x03U /* EMBEDDED_CONTROLLER */
#define GAS_ADDRESS_SPACE_ID_SMBUS  0x04U
#define GAS_ADDRESS_SPACE_ID_CMOS   0x05U
#define GAS_ADDRESS_SPACE_ID_PCIBAR 0x06U /* PCI_BAR_TARGET */
#define GAS_ADDRESS_SPACE_ID_IPMI   0x07U
#define GAS_ADDRESS_SPACE_ID_GPIO   0x08U
#define GAS_ADDRESS_SPACE_ID_GSB    0x09U /* Generic Serial Bus*/
#define GAS_ADDRESS_SPACE_ID_PCC    0x0aU /* Platform Communications Channel */
#define GAS_ADDRESS_SPACE_ID_PRM    0x0bU /* Platform Runtime Mechanism */

#define GAS_ACCESS_SIZE_UNDEFINED 0U
#define GAS_ACCESS_SIZE_BYTE      1U
#define GAS_ACCESS_SIZE_WORD      2U
#define GAS_ACCESS_SIZE_DWORD     3U
#define GAS_ACCESS_SIZE_QWORD     4U

typedef struct {
	u8 addressSpaceId;
	u8 registerWidth;
	u8 registerOffset;
	u8 accessSize;
	u64 address;
} __attribute__((packed)) hal_gas_t;

typedef struct {
	u8 addressSpaceId;
	u8 registerWidth;
	u8 registerOffset;
	u8 accessSize;
	void *address;
} hal_gasMapped_t;


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
} __attribute__((packed)) xsdp_t;


typedef struct {
	sdt_header_t header;
	addr_t localApicAddr;
	u32 flags;
	u8 entries[]; /* It is an array of variable length elements */
} __attribute__((packed)) hal_madtHeader_t;


typedef struct {
	sdt_header_t header;
	u32 eventTimerBlockID;
	hal_gas_t baseAddress;
	u8 hpetNumber;
	u16 minPeriodicClockTick;
	u8 pageProtection;
} __attribute__((packed)) hal_hpetHeader_t;


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
	hal_gas_t resetReg; /* RESET_REG */
	u8 resetValue;      /* RESET_VALUE */
	u16 armBootArch;    /* ARM_BOOT_ARCH */
	u8 fadtMinorVersion;
	u64 xFirmwareCtrl;         /* X_FIRMWARE_CTRL */
	u64 xDsdt;                 /* X_DSDT */
	hal_gas_t xPm1aEvtBlk;     /* X_PM1a_EVT_BLK */
	hal_gas_t xPm1bEvtBlk;     /* X_PM1b_EVT_BLK */
	hal_gas_t xPm1aCntBlk;     /* X_PM1a_CNT_BLK */
	hal_gas_t xPm1bCntBlk;     /* X_PM1b_CNT_BLK */
	hal_gas_t xPm2CntBlk;      /* X_PM2_CNT_BLK */
	hal_gas_t xPmTmrBlk;       /* X_PM_TMR_BLK */
	hal_gas_t xGpe0Blk;        /* X_GPE0_BLK */
	hal_gas_t xGpe1Blk;        /* X_GPE1_BLK */
	hal_gas_t sleepControlReg; /* SLEEP_CONTROL_REG */
	hal_gas_t sleepStatusReg;  /* SLEEP_STATUS_REG */
	u64 hypervisorVendorIdentity;

} __attribute__((packed)) hal_fadtHeader_t;


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
	hal_madtHeader_t *madt;
	hal_fadtHeader_t *fadt;
	hal_hpetHeader_t *hpet;
	void *devices; /* Address space, where memory mapped devices go */
	struct {
		u32 count;
		hal_memEntry_t entries[HAL_MEM_ENTRIES];
	} memMap;
} hal_config_t;


extern hal_config_t hal_config;


static inline int hal_isLapicPresent(void)
{
	return (int)(hal_config.localApicAddr != NULL);
}


static inline void _hal_lapicWrite(u32 reg, u32 value)
{
	*(volatile u32 *)(hal_config.localApicAddr + reg) = value;
}


static inline u32 _hal_lapicRead(u32 reg)
{
	return *(volatile u32 *)(hal_config.localApicAddr + reg);
}


void _hal_configInit(syspage_t *s);


void *_hal_configMapDevice(u32 *pdir, addr_t start, size_t size, vm_attr_t attr);


void _hal_gasAllocDevice(const hal_gas_t *gas, hal_gasMapped_t *mgas, size_t size);


int _hal_gasWrite32(hal_gasMapped_t *gas, u32 offset, u32 val);


int _hal_gasRead32(hal_gasMapped_t *gas, u32 offset, u32 *val);

#endif
