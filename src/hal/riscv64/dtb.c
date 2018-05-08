/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * DTB parser
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "string.h"
#include "dtb.h"
#include "pmap.h"

#include "../../../include/errno.h"


extern void _end(void);


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


struct {
	struct _fdt_header_t *fdth;

	void *start;

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
		u64 *reg;
	} memory;

} dtb_common;


char *dtb_getString(u32 i)
{
	return (char *)((void *)dtb_common.fdth + ntoh32(dtb_common.fdth->off_dt_strings) + i);
}


void dtb_parseSystem(void *dtb, u32 si, u32 l)
{
	if (!hal_strcmp(dtb_getString(si), "model"))
		dtb_common.model = dtb;
	else if (!hal_strcmp(dtb_getString(si), "compatible"))
		dtb_common.compatible = dtb;

	return;
}


void dtb_parseCPU(void *dtb, u32 si, u32 l)
{
	if (!hal_strcmp(dtb_getString(si), "compatible"))
		dtb_common.cpus[dtb_common.ncpus].compatible = dtb;
	else if (!hal_strcmp(dtb_getString(si), "riscv,isa"))
		dtb_common.cpus[dtb_common.ncpus].isa = dtb;
	else if (!hal_strcmp(dtb_getString(si), "mmu-type"))
		dtb_common.cpus[dtb_common.ncpus].mmu = dtb;
	else if (!hal_strcmp(dtb_getString(si), "clock-frequency"))
		dtb_common.cpus[dtb_common.ncpus].clock = ntoh32(*(u32 *)dtb);

	return;
}


void dtb_parseInterruptController(void *dtb, u32 si, u32 l)
{
	return;
}


int dtb_parseMemory(void *dtb, u32 si, u32 l)
{
	if (!hal_strcmp(dtb_getString(si), "reg")) {
		dtb_common.memory.nreg = l / 16;
		dtb_common.memory.reg = dtb;
	}
	return 0;
}


void dtb_parse(void *arg, void *dtb)
{
	u32 token, si;
	size_t l;
	unsigned int d = 0;
	enum { stateIdle = 0, stateSystem, stateCPU, stateInterruptController, stateMemory } state;
	extern char _start;

	/* Copy DTB into BSS */
	dtb_common.fdth = (struct _fdt_header_t *)dtb;
	hal_memcpy(_end, dtb, ntoh32(dtb_common.fdth->totalsize));
	dtb_common.fdth = (struct _fdt_header_t *)_end;

//	lib_printf("fdt_header.magic: %x\n", ntoh32(dtb_common.fdth->magic));

	dtb = (void *)dtb_common.fdth + ntoh32(dtb_common.fdth->off_dt_struct);

	state = stateIdle;

	for (;;) {
		token = ntoh32(*(u32 *)dtb);
		dtb += 4;

		/* FDT_BEGIN_NODE */
		if (token == 1) {
			if (!d && (*(char *)dtb == 0))
				state = stateSystem;
			else if ((d == 2) && !hal_strncmp(dtb, "cpu", 3))
				state = stateCPU;
			else if ((d == 3) && !hal_strncmp(dtb, "interrupt-controller", 20))
				state = stateInterruptController;
			else if ((d == 1) && !hal_strncmp(dtb, "memory", 6))
				state = stateMemory;

			dtb += ((hal_strlen(dtb) + 3) & ~3);
			d++;
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
			case stateCPU:
				dtb_parseCPU(dtb, si, l);
				break;
			case stateInterruptController:
				dtb_parseInterruptController(dtb, si, l);
				break;
			case stateMemory:
				dtb_parseMemory(dtb, si, l);
				break;
			case stateIdle:
				break;
			}

			dtb += l;
		}

		/* FDT_NODE_END */
		else if (token == 2) {
			d--;
			if (state == stateInterruptController)
				state = stateCPU;
			if (state == stateCPU) {
				dtb_common.ncpus++;
				state = stateSystem;
			}
		}
		else if (token == 9)
			break;
	}

//	lib_printf("model: %s (%s)\n", dtb_common.model, dtb_common.compatible);
//	lib_printf("cpu: %s@%dMHz(%s+%s)\n", dtb_common.cpus[0].compatible, dtb_common.cpus[0].clock / 1000000, dtb_common.cpus[0].isa, dtb_common.cpus[0].mmu);
	dtb_common.start = &_start;
}


static void *dtb_relocate(void *addr)
{
	return (addr + VADDR_KERNEL - ((u64)dtb_common.start /*& 0xffffffffc0000000*/));
}


const void dtb_getSystem(char **model, char **compatible)
{
	*model = dtb_relocate(dtb_common.model);
	*compatible = dtb_relocate(dtb_common.compatible);

	return;
}


int dtb_getCPU(unsigned int n, char **compatible, u32 *clock, char **isa, char **mmu)
{
	if (n >= dtb_common.ncpus)
		return -EINVAL;

	*compatible = dtb_relocate(dtb_common.cpus[n].compatible);
	*clock = dtb_common.cpus[n].clock;
	*isa = dtb_relocate(dtb_common.cpus[n].isa);
	*mmu = dtb_relocate(dtb_common.cpus[n].mmu);

	return EOK;
}


void dtb_getMemory(u64 **reg, size_t *nreg)
{
	*reg = dtb_relocate(dtb_common.memory.reg);
	*nreg = dtb_common.memory.nreg;
	return;
}


void dtb_getReservedMemory(u64 **reg)
{
	struct _fdt_header_t *fdth;

	fdth = dtb_relocate(dtb_common.fdth);

	*reg = (u64 *)((void *)fdth + ntoh32(fdth->off_mem_rsvmap));

	return;
}


void dtb_getDTBArea(u64 *dtb, u32 *dtbsz)
{
	struct _fdt_header_t *fdth;

	*dtb = (u64)dtb_common.fdth;

	fdth = dtb_relocate(dtb_common.fdth);
	*dtbsz = ntoh32(fdth->totalsize);

	return;
}
