/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * DTB parser
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "dtb.h"
#include "hal/string.h"
#include "hal/pmap.h"

#include "include/errno.h"

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


static struct {
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

	struct {
		struct {
			int exist;
			u32 *reg;
		} intctl;
	} soc;

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


void dtb_parseSOCInterruptController(void *dtb, u32 si, u32 l)
{
	dtb_common.soc.intctl.exist = 1;
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


#if 0
static void dtb_print(char *s) {
	while (*s != 0) {
		__asm__ (
			"li t0, 0x10000000;" \
			"ld t1, (%0);" \
			"sd t1, (t0);" \
			:: "r" (s) : "t0", "t1", "memory"
		);
		s++;
	}
}
#endif

void dtb_parse(void *arg, void *dtb)
{
	extern char _start;
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

	/* Copy DTB into BSS */
	dtb_common.fdth = (struct _fdt_header_t *)dtb;

	if (dtb_common.fdth->magic != ntoh32(0xd00dfeed))
		return;

	dtb = (void *)dtb_common.fdth + ntoh32(dtb_common.fdth->off_dt_struct);
	dtb_common.soc.intctl.exist = 0;
	dtb_common.ncpus = 0;

	for (;;) {
		token = ntoh32(*(u32 *)dtb);
		dtb += 4;

		/* FDT_NODE_BEGIN */
		if (token == 1) {
#if 0
			char buff[2] = " ";
			dtb_print(dtb);
			dtb_print(" ");
			buff[0] = '0' + d;
			dtb_print(buff);
			dtb_print(" ");
			buff[0] = '0' + state;
			dtb_print(buff);
			dtb_print("\n");
#endif

			if (!d && (*(char *)dtb == 0))
				state = stateSystem;
			else if ((d == 1) && !hal_strncmp(dtb, "memory@", 7))
				state = stateMemory;
			else if ((d == 2) && !hal_strncmp(dtb, "cpu@", 4))
				state = stateCPU;
			else if ((state == stateCPU) && !hal_strncmp(dtb, "interrupt-controller", 20))
				state = stateCPUInterruptController;
			else if ((d == 1) && !hal_strncmp(dtb, "soc", 3))
				state = stateSOC;
			else if ((state == stateSOC) && (!hal_strncmp(dtb, "interrupt-controller@", 21) || !hal_strncmp(dtb, "plic@", 5)))
				state = stateSOCInterruptController;

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

			case stateMemory:
				dtb_parseMemory(dtb, si, l);
				break;

			case stateCPU:
				dtb_parseCPU(dtb, si, l);
				break;

			case stateCPUInterruptController:
				dtb_parseInterruptController(dtb, si, l);
				break;

			case stateSOCInterruptController:
				dtb_parseSOCInterruptController(dtb, si, l);
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
				dtb_common.ncpus++;
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
				break;
			}
			d--;
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
	return (void *)((u64)((1L << 39) - (u64)2 * 1024 * 1024 * 1024 + addr - 0x80000000L) | (u64)0xffffff8000000000);
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


int dtb_getPLIC(void)
{
//	*reg = dtb_relocate(dtb_common.memory.reg);
	return dtb_common.soc.intctl.exist;
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
