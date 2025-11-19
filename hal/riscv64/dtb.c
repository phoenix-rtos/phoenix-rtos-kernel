/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * DTB parser
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "dtb.h"
#include "hal/string.h"

#include <arch/pmap.h>

#include "include/errno.h"


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

	char *model;
	char *compatible;

	size_t ncpus;
	struct {
		u32 reg;
		char *compatible;
		char *mmu;
		char *isa;
		u32 clock;

		struct {
			char *compatible;
		} intctl;
	} cpus[8];

	struct {
		size_t nreg;
		u8 *reg;
	} memory;

	struct {
		struct {
			int exist;
			u32 *reg;
		} intctl;
	} soc;

} dtb_common;


static char *dtb_getString(u32 i)
{
	return (char *)((void *)dtb_common.fdth + ntoh32(dtb_common.fdth->off_dt_strings) + i);
}


static void dtb_parseSystem(void *dtb, u32 si, u32 l)
{
	if (hal_strcmp(dtb_getString(si), "model") == 0) {
		dtb_common.model = dtb;
	}
	else if (hal_strcmp(dtb_getString(si), "compatible") == 0) {
		dtb_common.compatible = dtb;
	}
	else {
		/* No action required */
	}
}


static void dtb_parseCPU(void *dtb, u32 si, u32 l)
{
	if (hal_strcmp(dtb_getString(si), "compatible") == 0) {
		dtb_common.cpus[dtb_common.ncpus].compatible = dtb;
	}
	else if (hal_strcmp(dtb_getString(si), "riscv,isa") == 0) {
		dtb_common.cpus[dtb_common.ncpus].isa = dtb;
	}
	else if (hal_strcmp(dtb_getString(si), "mmu-type") == 0) {
		dtb_common.cpus[dtb_common.ncpus].mmu = dtb;
	}
	else if (hal_strcmp(dtb_getString(si), "clock-frequency") == 0) {
		dtb_common.cpus[dtb_common.ncpus].clock = ntoh32(*(u32 *)dtb);
	}
	else {
		/* No action required */
	}
}


static void dtb_parseInterruptController(void *dtb, u32 si, u32 l)
{
}


static void dtb_parseSOCInterruptController(void *dtb, u32 si, u32 l)
{
	dtb_common.soc.intctl.exist = 1;
}


static int dtb_parseMemory(void *dtb, u32 si, u32 l)
{
	if (hal_strcmp(dtb_getString(si), "reg") == 0) {
		dtb_common.memory.nreg = (size_t)l / 16U;
		dtb_common.memory.reg = dtb;
	}
	return 0;
}


void dtb_save(void *dtb)
{
	dtb_common.fdth = (struct _fdt_header_t *)dtb;
}


void dtb_parse(void)
{
	void *dtb;
	unsigned int d = 0;
	u32 token, si;
	size_t l;
	enum {
		stateIdle,
		stateSystem,
		stateCPU,
		stateCPUInterruptController,
		stateMemory,
		stateSOC,
		stateSOCInterruptController
	} state = stateIdle;

	if (dtb_common.fdth->magic != ntoh32(0xd00dfeedUL)) {
		return;
	}

	dtb = (void *)dtb_common.fdth + ntoh32(dtb_common.fdth->off_dt_struct);
	dtb_common.soc.intctl.exist = 0;
	dtb_common.ncpus = 0;

	for (;;) {
		token = ntoh32(*(u32 *)dtb);
		dtb += 4;

		/* FDT_NODE_BEGIN */
		if (token == 1U) {
			if ((d == 0U) && (*(char *)dtb == '\0')) {
				state = stateSystem;
			}
			else if ((d == 1U) && (hal_strncmp(dtb, "memory@", 7U) == 0)) {
				state = stateMemory;
			}
			else if ((d == 2U) && (hal_strncmp(dtb, "cpu@", 4U) == 0)) {
				state = stateCPU;
			}
			else if ((state == stateCPU) && (hal_strncmp(dtb, "interrupt-controller", 20U) == 0)) {
				state = stateCPUInterruptController;
			}
			else if ((d == 1U) && (hal_strncmp(dtb, "soc", 3U) == 0)) {
				state = stateSOC;
			}
			else if ((state == stateSOC) && ((hal_strncmp(dtb, "interrupt-controller@", 21U) == 0) || (hal_strncmp(dtb, "plic@", 5) == 0))) {
				state = stateSOCInterruptController;
			}
			else {
				/* No action required */
			}

			dtb += ((hal_strlen(dtb) + 3U) & ~3U);
			d++;
		}

		/* FDT_PROP */
		else if (token == 3U) {
			l = (size_t)(u32)(ntoh32(*(u32 *)dtb));
			l = ((l + 3U) & ~3U);

			dtb += 4;
			si = ntoh32(*(u32 *)dtb);
			dtb += 4;

			switch (state) {
				case stateSystem:
					dtb_parseSystem(dtb, si, (u32)l);
					break;

				case stateMemory:
					(void)dtb_parseMemory(dtb, si, (u32)l);
					break;

				case stateCPU:
					dtb_parseCPU(dtb, si, (u32)l);
					break;

				case stateCPUInterruptController:
					dtb_parseInterruptController(dtb, si, (u32)l);
					break;

				case stateSOCInterruptController:
					dtb_parseSOCInterruptController(dtb, si, (u32)l);
					break;

				default:
					/* No action required */
					break;
			}

			dtb += l;
		}

		/* FDT_NODE_END */
		else if (token == 2U) {
			switch (state) {
				/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
				case stateCPU:
					dtb_common.ncpus++;
					/* Fall-through */
				case stateMemory:
					state = stateSystem;
					break;

				case stateCPUInterruptController:
					state = stateCPU;
					break;

				case stateSOCInterruptController:
					state = stateSOC;
					break;

				default:
					/* No action required */
					break;
			}
			if (d == 0U) {
				/* Shouldn't happen */
				return;
			}
			d--;
		}
		else if (token == 9U) {
			break;
		}
		else {
			/* No action required */
		}
	}
}


void dtb_getSystem(char **model, char **compatible)
{
	*model = dtb_common.model;
	*compatible = dtb_common.compatible;
}


int dtb_getCPU(unsigned int n, char **compatible, u32 *clock, char **isa, char **mmu)
{
	if (n >= dtb_common.ncpus) {
		return -EINVAL;
	}

	*compatible = dtb_common.cpus[n].compatible;
	*clock = dtb_common.cpus[n].clock;
	*isa = dtb_common.cpus[n].isa;
	*mmu = dtb_common.cpus[n].mmu;

	return EOK;
}


void dtb_getMemory(u8 **reg, size_t *nreg)
{
	*reg = dtb_common.memory.reg;
	*nreg = dtb_common.memory.nreg;
}


int dtb_getPLIC(void)
{
	return dtb_common.soc.intctl.exist;
}


void dtb_getReservedMemory(u64 **reg)
{
	struct _fdt_header_t *fdth;

	fdth = dtb_common.fdth;

	*reg = (u64 *)((void *)fdth + ntoh32(fdth->off_mem_rsvmap));

	return;
}


void dtb_getDTBArea(u64 *dtb, u32 *dtbsz)
{
	*dtb = (u64)dtb_common.fdth - VADDR_DTB;
	*dtbsz = ntoh32(dtb_common.fdth->totalsize);
}


void _dtb_init(void)
{
	void *dtb = dtb_common.fdth;
	hal_memset(&dtb_common, 0, sizeof(dtb_common));

	/* DTB is mapped on giga-page */
	dtb_common.fdth = (void *)(((ptr_t)dtb & 0x3fffffffUL) + VADDR_DTB);

	dtb_parse();
}
