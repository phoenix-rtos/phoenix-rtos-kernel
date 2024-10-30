/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * DTB parser
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "dtb.h"
#include "hal/string.h"

#include <arch/pmap.h>
#include <arch/cpu.h>

#include "include/errno.h"

extern void _end(void);


#define MAX_CPUS 8
#define MAX_MEM_BANKS 8
#define MAX_SERIALS 4

struct _fdt_header_t {
	u32 magic;
	u32 totalsize;
	u32 off_dt_struct;
	u32 off_dt_strings;
	u32 off_mem_rsvmap;
	u32 version;
	u32 last_comp_version;
	u32 boot_cpuid_phys;
	u32 size_dt_strings;
	u32 size_dt_struct;
};


static struct {
	struct _fdt_header_t *fdth;

	void *start;

	char *model;
	char *compatible;

	size_t nCpus;
	struct {
		char *compatible;
		u32 clock; /* TODO: on ZynqMP this is not populated */
	} cpus[MAX_CPUS];

	size_t nMemBanks;
	dtb_memBank_t memBanks[MAX_MEM_BANKS];

	struct {
		int version;
		addr_t gicd;
		addr_t gicc;
	} apu_gic;

	size_t nSerials;
	dtb_serial_t serials[MAX_SERIALS];
} dtb_common;


static char *dtb_getString(u32 i)
{
	return (char *)((void *)dtb_common.fdth + ntoh32(dtb_common.fdth->off_dt_strings) + i);
}


static void dtb_parseSystem(void *dtb, u32 si, u32 l)
{
	if (!hal_strcmp(dtb_getString(si), "model")) {
		dtb_common.model = dtb;
	}
	else if (!hal_strcmp(dtb_getString(si), "compatible")) {
		dtb_common.compatible = dtb;
	}
}


static void dtb_parseCPU(void *dtb, u32 si, u32 l)
{
	if (!hal_strcmp(dtb_getString(si), "compatible")) {
		dtb_common.cpus[dtb_common.nCpus].compatible = dtb;
	}
	else if (!hal_strcmp(dtb_getString(si), "clock-frequency")) {
		dtb_common.cpus[dtb_common.nCpus].clock = ntoh32(*(u32 *)dtb);
	}
}


static void dtb_parseInterruptController(void *dtb, u32 si, u32 l)
{
	u64 gicc, gicd;
	if (!hal_strcmp(dtb_getString(si), "reg")) {
		if (l >= 32) {
			hal_memcpy(&gicd, ((u64*)dtb + 0), 8);
			gicd = ntoh64(gicd);
			hal_memcpy(&gicc, ((u64*)dtb + 2), 8);
			gicc = ntoh64(gicc);
			dtb_common.apu_gic.gicd = gicd;
			dtb_common.apu_gic.gicc = gicc;
		}
	}
	else if (!hal_strcmp(dtb_getString(si), "revision")) {
		if ((l == 4) && (sizeof(dtb_common.apu_gic.version) == 4)) {
			hal_memcpy(&dtb_common.apu_gic.version, dtb, sizeof(dtb_common.apu_gic.version));
			dtb_common.apu_gic.version = ntoh32(dtb_common.apu_gic.version);
		}
	}
}


static void dtb_parseSerial(void *dtb, u32 si, u32 l)
{
	u64 base;
	u32 nIntrs, i, intr;
	if (!hal_strcmp(dtb_getString(si), "reg")) {
		if (l >= 8) {
			hal_memcpy(&base, dtb, 8);
			base = ntoh64(base);
			dtb_common.serials[dtb_common.nSerials].base = base;
		}
	}
	else if (!hal_strcmp(dtb_getString(si), "interrupts")) {
		nIntrs = l / 4;
		for (i = 0; i < nIntrs && i < 4; i++) {
			hal_memcpy(&intr, (u32*)dtb + i, 4);
			dtb_common.serials[dtb_common.nSerials].intrs[i] = ntoh32(intr);
		}
	}
}


static int dtb_parseDDRBank(void *dtb, u32 si, u32 l)
{
	addr_t start = 0, size = 0;
	if (!hal_strcmp(dtb_getString(si), "reg")) {
		/* TODO: the proper way to get format for "reg" would be to check the "compatible" property of the node */
		if (l == 12) {
			hal_memcpy(&start, (u32*)dtb + 1, 4);
			start = ntoh32(start);
			hal_memcpy(&size, (u32*)dtb + 2, 4);
			size = ntoh32(size);
		}
		else if (l == 16) {
			hal_memcpy(&start, (u64*)dtb + 0, 4);
			start = ntoh64(start);
			hal_memcpy(&size, (u64*)dtb + 1, 4);
			size = ntoh64(size);
		}
		else {
			return 0;
		}

		dtb_common.memBanks[dtb_common.nMemBanks].start = start;
		dtb_common.memBanks[dtb_common.nMemBanks].end = start + size - 1;
	}
	return 0;
}


void dtb_parse(void)
{
	void *dtb;
	unsigned int depth = 0;
	u32 token, si;
	size_t l;
	enum {
		stateIdle,
		stateSystem,
		stateCPU,
		stateAPU_GIC,
		stateInterruptController,
		stateDDRBank,
		stateSerial,
	} state = stateIdle;

	if (dtb_common.fdth->magic != ntoh32(0xd00dfeed)) {
		return;
	}

	dtb = (void *)dtb_common.fdth + ntoh32(dtb_common.fdth->off_dt_struct);

	for (;;) {
		token = ntoh32(*(u32 *)dtb);
		dtb += 4;

		/* FDT_NODE_BEGIN */
		if (token == 1) {
			if ((depth == 0) && (*(char *)dtb == 0)) {
				state = stateSystem;
			}
			else if ((depth == 1) && (hal_strncmp(dtb, "ddr_bank", 8) == 0)) {
				if (dtb_common.nMemBanks < MAX_MEM_BANKS) {
					state = stateDDRBank;
				}
			}
			else if ((depth == 1) && (hal_strncmp(dtb, "amba_apu_gic@", 13) == 0)) {
				state = stateAPU_GIC;
			}
			else if ((depth == 2) && (hal_strncmp(dtb, "apu_cpu@", 8) == 0)) {
				if (dtb_common.nCpus < MAX_CPUS) {
					state = stateCPU;
				}
			}
			else if ((state == stateAPU_GIC) && (hal_strncmp(dtb, "interrupt-controller@", 21) == 0)) {
				state = stateInterruptController;
			}
			else if ((depth == 2) && (hal_strncmp(dtb, "serial@", 7) == 0)) {
				if (dtb_common.nSerials < MAX_SERIALS) {
					state = stateSerial;
					hal_memset(dtb_common.serials[dtb_common.nSerials].intrs, 0xFF, sizeof(dtb_common.serials[dtb_common.nSerials].intrs));
				}
			}

			dtb += ((hal_strlen(dtb) + 3) & ~3);
			depth++;
		}

		/* FDT_PROP */
		else if (token == 3) {
			l = ntoh32(*(u32 *)dtb);
			l = ((l + 3) & ~3);

			dtb += 4;
			si = ntoh32(*(u32 *)dtb);
			dtb += 4;

			switch (state) {
				case stateSystem:
					dtb_parseSystem(dtb, si, l);
					break;

				case stateDDRBank:
					dtb_parseDDRBank(dtb, si, l);
					break;

				case stateInterruptController:
					dtb_parseInterruptController(dtb, si, l);
					break;

				case stateCPU:
					dtb_parseCPU(dtb, si, l);
					break;

				case stateSerial:
					dtb_parseSerial(dtb, si, l);
					break;

				default:
					break;
			}

			dtb += l;
		}

		/* FDT_NODE_END */
		else if (token == 2) {
			switch (state) {
				case stateCPU:
					dtb_common.nCpus++;
					state = stateIdle;
					break;

				case stateDDRBank:
					dtb_common.nMemBanks++;
					state = stateIdle;
					break;

				case stateSerial:
					dtb_common.nSerials++;
					state = stateIdle;
					break;

				default:
					state = stateIdle;
					break;
			}
			depth--;
		}
		else if (token == 9) {
			break;
		}
	}
}


void dtb_getSystem(char **model, char **compatible)
{
	*model = dtb_common.model;
	*compatible = dtb_common.compatible;
}


int dtb_getCPU(unsigned int n, char **compatible, u32 *clock)
{
	if (n >= dtb_common.nCpus) {
		return -EINVAL;
	}

	*compatible = dtb_common.cpus[n].compatible;
	*clock = dtb_common.cpus[n].clock;

	return EOK;
}


void dtb_getMemory(dtb_memBank_t **banks, size_t *nBanks)
{
	*banks = dtb_common.memBanks;
	*nBanks = dtb_common.nMemBanks;
}


void dtb_getGIC(int *version, addr_t *gicc, addr_t *gicd)
{
	*version = dtb_common.apu_gic.version;
	*gicc = dtb_common.apu_gic.gicc;
	*gicd = dtb_common.apu_gic.gicd;
}


void dtb_getSerials(dtb_serial_t **serials, size_t *nSerials)
{
	*serials = dtb_common.serials;
	*nSerials = dtb_common.nSerials;
}


void _dtb_init(addr_t dtbPhys)
{
	hal_memset(&dtb_common, 0, sizeof(dtb_common));
	dtb_common.fdth = (void *)((dtbPhys & (SIZE_PAGE - 1)) + VADDR_DTB);

	dtb_parse();
}
