/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32N6 RISAF (Resource isolation slave unit for address space protection (full version)) configuration.
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/armv8m/stm32/stm32.h"
#include "hal/armv8m/stm32/n6/stm32n6_regs.h"
#include "include/errno.h"
#include "hal/cpu.h"


#define RISAF1_BASE  ((void *)0x54026000U)
#define RISAF2_BASE  ((void *)0x54027000U)
#define RISAF3_BASE  ((void *)0x54028000U)
#define RISAF4_BASE  ((void *)0x54029000U)
#define RISAF5_BASE  ((void *)0x5402a000U)
#define RISAF6_BASE  ((void *)0x5402b000U)
#define RISAF7_BASE  ((void *)0x5402c000U)
#define RISAF8_BASE  ((void *)0x5402d000U)
#define RISAF9_BASE  ((void *)0x5402e000U)
#define RISAF11_BASE ((void *)0x54030000U)
#define RISAF12_BASE ((void *)0x54031000U)
#define RISAF13_BASE ((void *)0x54032000U)
#define RISAF14_BASE ((void *)0x54033000U)
#define RISAF15_BASE ((void *)0x54034000U)
#define RISAF21_BASE ((void *)0x54035000U)
#define RISAF22_BASE ((void *)0x54036000U)
#define RISAF23_BASE ((void *)0x54037000U)

/* TODO: support for IAC (illegal access controller) would be nice to have for debugging,
 * but it is not vital. */
#define IAC_BASE ((void *)0x54025000U)


enum {
	risaf_tcm = 0,
	risaf_axisram0,
	risaf_axisram1,
	risaf_npu_mst0,
	risaf_npu_mst1,
	risaf_cpu_mst,
	risaf_flexram,
	risaf_cacheaxi,
	risaf_vencram,
	risaf_xspi1,
	risaf_xspi2,
	risaf_xspi3,
	risaf_fmc,
	risaf_cache_config,
	risaf_ahbram1,
	risaf_ahbram2,
	risaf_bkpsram,
};


static const struct {
	volatile u32 *base;
	u32 start;       /* First protected address (within CPU address space) */
	u32 end;         /* Last protected address (within CPU address space) */
	u32 granularity; /* Granularity of region as bit mask */
	int pctl;        /* -1 - RISAF is always on, otherwise - check the given peripheral before trying to configure */
	u8 n_regions;    /* Number of regions supported */
	u8 isCIDAware;   /* 1 for firewalls that can do CID-based filtering */
} risafs[17] = {
	[risaf_tcm] = {
		.base = RISAF1_BASE,
		.start = 0x00000000U,
		.end = 0x3fffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = -1,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_axisram0] = {
		.base = RISAF2_BASE,
		.start = 0x34000000U,
		.end = 0x341fffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_axisram1, /* Difference in name is intentional; RISAF2 (AXISRAM0) protects AXISRAM1 */
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_axisram1] = {
		.base = RISAF3_BASE,
		.start = 0x34100000U,
		.end = 0x341fffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_axisram2, /* Difference in name is intentional; RISAF3 (AXISRAM1) protects AXISRAM2 */
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_npu_mst0] = {
		/* TODO: needs verification - NPU has to be turned on */
		.base = RISAF4_BASE,
		.start = 0x0U,
		.end = 0xffffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_npu,
		.n_regions = 11U,
		.isCIDAware = 1U,
	},
	[risaf_npu_mst1] = {
		/* TODO: needs verification - NPU has to be turned on */
		.base = RISAF5_BASE,
		.start = 0x0,
		.end = 0xffffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_npu,
		.n_regions = 11U,
		.isCIDAware = 1U,
	},
	[risaf_cpu_mst] = {
		.base = RISAF6_BASE,
		.start = 0x0U,
		.end = 0xffffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = -1,
		.n_regions = 11,
		.isCIDAware = 1,
	},
	[risaf_flexram] = {
		.base = RISAF7_BASE,
		.start = 0x34000000U,
		.end = 0x3407ffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_flexram,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_cacheaxi] = {
		/* NOTE: on illegal accesses, the address returned starts from 0x353c0000 instead of 0x343c0000 */
		.base = RISAF8_BASE,
		.start = 0x343c0000U,
		.end = 0x343fffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_npucacheram,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_vencram] = {
		.base = RISAF9_BASE,
		.start = 0x34400000U,
		.end = 0x3441ffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_vencram,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_xspi1] = {
		.base = RISAF11_BASE,
		.start = 0x90000000U,
		.end = 0x9fffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_xspi1,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_xspi2] = {
		.base = RISAF12_BASE,
		.start = 0x70000000U,
		.end = 0x7fffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_xspi2,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_xspi3] = {
		.base = RISAF13_BASE,
		.start = 0x80000000U,
		.end = 0x8fffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_xspi3,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_fmc] = {
		.base = RISAF14_BASE,
		.start = 0x60000000U,
		.end = 0x6fffffffU,
		.granularity = (1UL << 12) - 1U,
		.pctl = pctl_fmc,
		.n_regions = 7U,
		.isCIDAware = 1U,
	},
	[risaf_cache_config] = {
		.base = RISAF15_BASE,
		.start = 0x580df000U,
		.end = 0x580dffffU,
		.granularity = (1UL << 2) - 1U,
		.pctl = pctl_npucache,
		.n_regions = 2U,
		.isCIDAware = 0U,
	},
	[risaf_ahbram1] = {
		.base = RISAF21_BASE,
		.start = 0x38000000U,
		.end = 0x38003fffU,
		.granularity = (1UL << 9) - 1U,
		.pctl = pctl_ahbsram1,
		.n_regions = 7U,
		.isCIDAware = 0U,
	},
	[risaf_ahbram2] = {
		.base = RISAF22_BASE,
		.start = 0x38004000U,
		.end = 0x38007fffU,
		.granularity = (1UL << 9) - 1U,
		.pctl = pctl_ahbsram2,
		.n_regions = 7U,
		.isCIDAware = 0U,
	},
	[risaf_bkpsram] = {
		.base = RISAF23_BASE,
		.start = 0x3c000000U,
		.end = 0x3c001fffU,
		.granularity = (1UL << 9) - 1U,
		.pctl = pctl_bkpsram,
		.n_regions = 3U,
		.isCIDAware = 0U,
	},
};


/* Configure a protection region in RISAF.
 * Note that `start` and `end` are addresses in the CPU address space, not offsets within the
 * domain of the selected RISAF.
 * `risaf` - Number of RISAF module (1 ~ 23)
 * `region` - Number of region (1 based)
 * `start` - Start of region (address of first byte). Must be aligned to granularity supported by module.
 * `end` - End of region (address of last byte). Must be aligned to granularity supported by module.
 * `privCIDMask` - Bit mask of which CIDs are permitted to only make privileged accesses.
 * 	 E.g. privCIDMask == 0x02 -> CID 1 can make only privileged accesses, other CIDs can make privileged
 *   or unprivileged accesses.
 * `readCIDMask` - Bit mask of which CIDs are permitted to read.
 * `writeCIDMask` - Bit mask of which CIDs are permitted to write.
 * `secure` - 0 - Region permits only non-secure accesses, 1 - Region permits only secure accesses.
 * 	 NOTE: secure == 0 forces all sub-regions to also be non-secure.
 * `enable` - 0 - Region disabled (default permissions apply instead), 1 - Region enabled
 */
int _stm32_risaf_configRegion(int risaf, u8 region, u32 start, u32 end, u8 privCIDMask, u8 readCIDMask, u8 writeCIDMask, int secure, int enable)
{
	const u32 region_offs = ((u32)region - 1U) * 0x10U;
	u32 tmp, status, lpStatus;
	if ((unsigned int)risaf >= (sizeof(risafs) / sizeof(risafs[0]))) {
		return -EINVAL;
	}

	/* Check if the corresponding memory or peripheral is turned on.
	 * Trying to configure RISAF for a peripheral that is off will result in a crash. */
	if (risafs[risaf].pctl >= 0) {
		if (_stm32_rccGetDevClock(risafs[risaf].pctl, &status, &lpStatus) < 0) {
			return -EINVAL;
		}

		if (status == 0U) {
			return -ENODEV;
		}
	}

	if ((region == 0U) || (region > risafs[risaf].n_regions)) {
		return -EINVAL;
	}

	if ((start < risafs[risaf].start) || (start > risafs[risaf].end) || (end < risafs[risaf].start) || (end > risafs[risaf].end)) {
		return -EINVAL;
	}

	if (((start & risafs[risaf].granularity) != 0U) || ((end & risafs[risaf].granularity) != risafs[risaf].granularity)) {
		return -EINVAL;
	}

	if (risafs[risaf].isCIDAware == 0U) {
		privCIDMask = (privCIDMask != 0U) ? 0xffU : 0U;
		readCIDMask = (readCIDMask != 0U) ? 0xffU : 0U;
		writeCIDMask = (writeCIDMask != 0U) ? 0xffU : 0U;
	}

	hal_cpuDataMemoryBarrier();
	tmp = *(risafs[risaf].base + risaf_reg1_cidcfgr + region_offs);
	tmp &= ~(0xffUL << 16);
	tmp |= ((u32)writeCIDMask) << 16;
	tmp &= ~0xffUL;
	tmp |= readCIDMask;
	*(risafs[risaf].base + risaf_reg1_cidcfgr + region_offs) = tmp;
	/* Values in registers are not CPU addresses, but offsets within the module's own address space. */
	start -= risafs[risaf].start;
	end -= risafs[risaf].start;
	*(risafs[risaf].base + risaf_reg1_startr + region_offs) = start;
	*(risafs[risaf].base + risaf_reg1_endr + region_offs) = end;
	hal_cpuDataMemoryBarrier();

	tmp = *(risafs[risaf].base + risaf_reg1_cfgr + region_offs);
	if (secure != 0) {
		tmp |= 1UL << 8;
	}
	else {
		tmp &= ~(1UL << 8);
	}

	if (enable != 0) {
		tmp |= 1U;
	}
	else {
		tmp &= ~1U;
	}

	tmp &= ~(0xffUL << 16);
	tmp |= ((u32)privCIDMask) << 16;
	*(risafs[risaf].base + risaf_reg1_cfgr + region_offs) = tmp;
	hal_cpuDataMemoryBarrier();

	return EOK;
}

static int _stm32_risaf_getFirstFreeRegion(unsigned int risaf)
{
	u32 region, region_offs;
	if (risaf >= (sizeof(risafs) / sizeof(risafs[0]))) {
		return -EINVAL;
	}

	for (region = 1U; region <= risafs[risaf].n_regions; region++) {
		region_offs = (region - 1U) * 0x10U;
		if ((*(risafs[risaf].base + risaf_reg1_cfgr + region_offs) & 1U) == 0U) {
			return (int)region;
		}
	}

	return -ENOMEM;
}


/* This structure maps regions in memory to IDs of RISAF that needs to be configured to achieve protection
 * on the given address range.
 * A RISAF may protect multiple address ranges and one address range may need multiple RISAFs to be set up correctly.
 * TODO: This default configuration is intended as a temporary measure to get DMA working. Ultimately
 * protection zones should be configurable in a similar manner to MPU regions.
 */
static const struct {
	u32 start; /* First address of the protection zone */
	u32 end;   /* Last address of the protection zone */
	int risaf; /* ID of the firewall that needs to be set up */
	u8 privCIDMask;
	u8 readCIDMask;
	u8 writeCIDMask;
	int secure;
} risaf_defConfig[] = {
	/* TCMs are accessed through the CPU - to configure protection, both CPU and TCM firewalls need to be configured */
	{ 0x10000000U, 0x1003ffffU, risaf_tcm, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x10000000U, 0x1003ffffU, risaf_cpu_mst, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x30000000U, 0x3003ffffU, risaf_tcm, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x30000000U, 0x3003ffffU, risaf_cpu_mst, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x34000000U, 0x34063fffU, risaf_flexram, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x34064000U, 0x340fffffU, risaf_axisram0, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x34100000U, 0x341fffffU, risaf_axisram1, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x34200000U, 0x343bffffU, risaf_cpu_mst, 0x0U, 0xffU, 0xffU, 1 }, /* AXISRAM3~6 are accessed through the CPU */
	{ 0x34200000U, 0x343bffffU, risaf_npu_mst0, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x34200000U, 0x343bffffU, risaf_npu_mst1, 0x0U, 0xffU, 0xffU, 1 }, /* AXISRAM3~6 are accessed through the NPU */
	{ 0x343c0000U, 0x343fffffU, risaf_cacheaxi, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x34400000U, 0x3441ffffU, risaf_vencram, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x38000000U, 0x38003fffU, risaf_ahbram1, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x38004000U, 0x38007fffU, risaf_ahbram2, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x70000000U, 0x7fffffffU, risaf_xspi2, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x80000000U, 0x8fffffffU, risaf_xspi3, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x90000000U, 0x9fffffffU, risaf_xspi1, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x90000000U, 0x9fffffffU, risaf_npu_mst0, 0x0U, 0xffU, 0xffU, 1 },
	{ 0x90000000U, 0x9fffffffU, risaf_npu_mst1, 0x0U, 0xffU, 0xffU, 1 }, /* PSRAM is accessed through the NPU */
};


/* This function configures RISAF modules to allow unprivileged or privileged, secure-only, read and write access
 * from all masters to all memories. */
int _stm32_risaf_init(void)
{
	unsigned int i;
	int region;
	for (i = 0U; i < sizeof(risaf_defConfig) / sizeof(risaf_defConfig[0]); i++) {
		region = _stm32_risaf_getFirstFreeRegion((unsigned int)risaf_defConfig[i].risaf);
		if (region < 0) {
			return -ENOMEM;
		}

		(void)_stm32_risaf_configRegion(
				risaf_defConfig[i].risaf,
				(u8)region,
				risaf_defConfig[i].start,
				risaf_defConfig[i].end,
				risaf_defConfig[i].privCIDMask,
				risaf_defConfig[i].readCIDMask,
				risaf_defConfig[i].writeCIDMask,
				risaf_defConfig[i].secure,
				1);
	}

	return EOK;
}
