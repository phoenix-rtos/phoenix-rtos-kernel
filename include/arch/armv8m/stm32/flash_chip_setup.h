/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * External Flash setup information structure
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _FLASH_CHIP_SETUP_H_
#define _FLASH_CHIP_SETUP_H_

#ifndef FLASHCS_FORMAT
#define FLASHCS_FORMAT "_flashcs_p%u"
#endif

/* Flash chip setup structure is intended to be used like this:
 * The file begins with the header block. The first uint32 of the header must be the version.
 * Version indicates major changes that break backwards compatibility.
 * In `flashcs_v1_header_t` the `features` field is a bitfield that indicates
 * which feature blocks follow.
 * Feature blocks follow the header block in the same order as indicated in the
 * `features` bitfield (LSB first).
 * If a feature is not present, the next feature which is present follows immediately
 * (no empty/unused space is left for features which are not present).
 * It is intended that new features will be added in order and software which intends
 * to use newer features will be updated to skip over unneeded features.
 */

#define FLASHCS_VER_1      1UL
#define FLASHCS_FEAT_BASIC (1UL << 0)

typedef unsigned char flashcs_uchar_t;     /* Use for 8 bit values */
typedef unsigned long int flashcs_ulong_t; /* Use for 32 bit values */

typedef struct {
	flashcs_ulong_t ccr;
	flashcs_ulong_t tcr;
	flashcs_ulong_t ir;
} flashcs_regs_v1_t __attribute__((packed));

typedef struct {
	flashcs_ulong_t version;
	flashcs_ulong_t features;
} flashcs_v1_header_t __attribute__((packed));

typedef struct {
	char name[32];
	flashcs_uchar_t jedecID[6];
	flashcs_uchar_t log_chipSize;
	flashcs_uchar_t log_eraseSize;
	flashcs_uchar_t log_pageSize;
	flashcs_ulong_t eraseTimeoutMs;
	flashcs_ulong_t chipEraseTimeoutMs;
	flashcs_ulong_t writePageTimeoutUs;
	flashcs_regs_v1_t read;
	flashcs_regs_v1_t write;
	flashcs_regs_v1_t erase;
	flashcs_regs_v1_t chipErase;
	flashcs_regs_v1_t writeEnable;
	flashcs_regs_v1_t writeDisable;
	flashcs_regs_v1_t readStatus;
	flashcs_ulong_t readStatus_addr;
	flashcs_uchar_t readStatus_dataLen;
} flashcs_v1_basic_t __attribute__((packed));

#endif /* _FLASH_CHIP_SETUP_H_ */
