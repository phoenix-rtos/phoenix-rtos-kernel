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


#define RISAF1_BASE  ((void *)0x54026000)
#define RISAF2_BASE  ((void *)0x54027000)
#define RISAF3_BASE  ((void *)0x54028000)
#define RISAF4_BASE  ((void *)0x54029000)
#define RISAF5_BASE  ((void *)0x5402a000)
#define RISAF6_BASE  ((void *)0x5402b000)
#define RISAF7_BASE  ((void *)0x5402c000)
#define RISAF8_BASE  ((void *)0x5402d000)
#define RISAF9_BASE  ((void *)0x5402e000)
#define RISAF11_BASE ((void *)0x54030000)
#define RISAF12_BASE ((void *)0x54031000)
#define RISAF13_BASE ((void *)0x54032000)
#define RISAF14_BASE ((void *)0x54033000)
#define RISAF15_BASE ((void *)0x54034000)
#define RISAF21_BASE ((void *)0x54035000)
#define RISAF22_BASE ((void *)0x54036000)
#define RISAF23_BASE ((void *)0x54037000)

/* TODO: support for IAC (illegal access controller) would be nice to have for debugging,
 * but it is not vital. */
#define IAC_BASE ((void *)0x54025000)


enum risafs {
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
} risafs[] = {
	[risaf_tcm] = {
		.base = RISAF1_BASE,
		.start = 0x00000000,
		.end = 0x3fffffff,
		.granularity = (1 << 12) - 1,
		.pctl = -1,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_axisram0] = {
		.base = RISAF2_BASE,
		.start = 0x34000000,
		.end = 0x341fffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_axisram1, /* Difference in name is intentional; RISAF2 (AXISRAM0) protects AXISRAM1 */
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_axisram1] = {
		.base = RISAF3_BASE,
		.start = 0x34100000,
		.end = 0x341fffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_axisram2, /* Difference in name is intentional; RISAF3 (AXISRAM1) protects AXISRAM2 */
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_npu_mst0] = {
		/* TODO: needs verification - NPU has to be turned on */
		.base = RISAF4_BASE,
		.start = 0x0,
		.end = 0xffffffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_npu,
		.n_regions = 11,
		.isCIDAware = 1,
	},
	[risaf_npu_mst1] = {
		/* TODO: needs verification - NPU has to be turned on */
		.base = RISAF5_BASE,
		.start = 0x0,
		.end = 0xffffffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_npu,
		.n_regions = 11,
		.isCIDAware = 1,
	},
	[risaf_cpu_mst] = {
		.base = RISAF6_BASE,
		.start = 0x0,
		.end = 0xffffffff,
		.granularity = (1 << 12) - 1,
		.pctl = -1,
		.n_regions = 11,
		.isCIDAware = 1,
	},
	[risaf_flexram] = {
		.base = RISAF7_BASE,
		.start = 0x34000000,
		.end = 0x3407ffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_flexram,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_cacheaxi] = {
		/* NOTE: on illegal accesses, the address returned starts from 0x353c0000 instead of 0x343c0000 */
		.base = RISAF8_BASE,
		.start = 0x343c0000,
		.end = 0x343fffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_npucacheram,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_vencram] = {
		.base = RISAF9_BASE,
		.start = 0x34400000,
		.end = 0x3441ffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_vencram,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_xspi1] = {
		.base = RISAF11_BASE,
		.start = 0x90000000,
		.end = 0x9fffffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_xspi1,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_xspi2] = {
		.base = RISAF12_BASE,
		.start = 0x70000000,
		.end = 0x7fffffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_xspi2,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_xspi3] = {
		.base = RISAF13_BASE,
		.start = 0x80000000,
		.end = 0x8fffffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_xspi3,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_fmc] = {
		.base = RISAF14_BASE,
		.start = 0x60000000,
		.end = 0x6fffffff,
		.granularity = (1 << 12) - 1,
		.pctl = pctl_fmc,
		.n_regions = 7,
		.isCIDAware = 1,
	},
	[risaf_cache_config] = {
		.base = RISAF15_BASE,
		.start = 0x580df000,
		.end = 0x580dffff,
		.granularity = (1 << 2) - 1,
		.pctl = pctl_npucache,
		.n_regions = 2,
		.isCIDAware = 0,
	},
	[risaf_ahbram1] = {
		.base = RISAF21_BASE,
		.start = 0x38000000,
		.end = 0x38003fff,
		.granularity = (1 << 9) - 1,
		.pctl = pctl_ahbsram1,
		.n_regions = 7,
		.isCIDAware = 0,
	},
	[risaf_ahbram2] = {
		.base = RISAF22_BASE,
		.start = 0x38004000,
		.end = 0x38007fff,
		.granularity = (1 << 9) - 1,
		.pctl = pctl_ahbsram2,
		.n_regions = 7,
		.isCIDAware = 0,
	},
	[risaf_bkpsram] = {
		.base = RISAF23_BASE,
		.start = 0x3c000000,
		.end = 0x3c001fff,
		.granularity = (1 << 9) - 1,
		.pctl = pctl_bkpsram,
		.n_regions = 3,
		.isCIDAware = 0,
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
int _stm32_risaf_configRegion(unsigned int risaf, u8 region, u32 start, u32 end, u8 privCIDMask, u8 readCIDMask, u8 writeCIDMask, int secure, int enable)
{
	const u32 region_offs = (region - 1) * 0x10;
	u32 tmp, status, lpStatus;
	if (risaf >= (sizeof(risafs) / sizeof(risafs[0]))) {
		return -EINVAL;
	}

	/* Check if the corresponding memory or peripheral is turned on.
	 * Trying to configure RISAF for a peripheral that is off will result in a crash. */
	if (risafs[risaf].pctl >= 0) {
		if (_stm32_rccGetDevClock(risafs[risaf].pctl, &status, &lpStatus) < 0) {
			return -EINVAL;
		}

		if (status == 0) {
			return -ENODEV;
		}
	}

	if ((region == 0) || (region > risafs[risaf].n_regions)) {
		return -EINVAL;
	}

	if ((start < risafs[risaf].start) || (start > risafs[risaf].end) || (end < risafs[risaf].start) || (end > risafs[risaf].end)) {
		return -EINVAL;
	}

	if (((start & risafs[risaf].granularity) != 0) || ((end & risafs[risaf].granularity) != risafs[risaf].granularity)) {
		return -EINVAL;
	}

	if (risafs[risaf].isCIDAware == 0) {
		privCIDMask = (privCIDMask != 0) ? 0xff : 0;
		readCIDMask = (readCIDMask != 0) ? 0xff : 0;
		writeCIDMask = (writeCIDMask != 0) ? 0xff : 0;
	}

	hal_cpuDataMemoryBarrier();
	tmp = *(risafs[risaf].base + risaf_reg1_cidcfgr + region_offs);
	tmp &= ~(0xff << 16);
	tmp |= ((u32)writeCIDMask) << 16;
	tmp &= ~0xff;
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
		tmp |= 1 << 8;
	}
	else {
		tmp &= ~(1 << 8);
	}

	if (enable) {
		tmp |= 1;
	}
	else {
		tmp &= ~1;
	}

	tmp &= ~(0xff << 16);
	tmp |= ((u32)privCIDMask) << 16;
	*(risafs[risaf].base + risaf_reg1_cfgr + region_offs) = tmp;
	hal_cpuDataMemoryBarrier();

	return EOK;
}

int _stm32_risaf_getFirstDisabledRegion(int risaf)
{
	u32 region, region_offs;
	if (risaf >= (sizeof(risafs) / sizeof(risafs[0]))) {
		return -EINVAL;
	}

	for (region = 1; region <= risafs[risaf].n_regions; region++) {
		region_offs = (region - 1) * 0x10;
		if ((*(risafs[risaf].base + risaf_reg1_cfgr + region_offs) & 1) == 0) {
			return region;
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
	u32 start;         /* First address of the protection zone */
	u32 end;           /* Last address of the protection zone */
	enum risafs risaf; /* ID of the firewall that needs to be set up */
	u8 privCIDMask;
	u8 readCIDMask;
	u8 writeCIDMask;
	u8 secure;
} risaf_defConfig[] = {
	/* TCMs are accessed through the CPU - to configure protection, both CPU and TCM firewalls need to be configured */
	{ 0x10000000, 0x1003ffff, risaf_tcm, 0x0, 0xff, 0xff, 1 },
	{ 0x10000000, 0x1003ffff, risaf_cpu_mst, 0x0, 0xff, 0xff, 1 },
	{ 0x30000000, 0x3003ffff, risaf_tcm, 0x0, 0xff, 0xff, 1 },
	{ 0x30000000, 0x3003ffff, risaf_cpu_mst, 0x0, 0xff, 0xff, 1 },
	{ 0x34000000, 0x34063fff, risaf_flexram, 0x0, 0xff, 0xff, 1 },
	{ 0x34064000, 0x340fffff, risaf_axisram0, 0x0, 0xff, 0xff, 1 },
	{ 0x34100000, 0x341fffff, risaf_axisram1, 0x0, 0xff, 0xff, 1 },
	{ 0x34200000, 0x343bffff, risaf_cpu_mst, 0x0, 0xff, 0xff, 1 }, /* AXISRAM3~6 are accessed through the CPU */
	{ 0x343c0000, 0x343fffff, risaf_cacheaxi, 0x0, 0xff, 0xff, 1 },
	{ 0x34400000, 0x3441ffff, risaf_vencram, 0x0, 0xff, 0xff, 1 },
	{ 0x38000000, 0x38003fff, risaf_ahbram1, 0x0, 0xff, 0xff, 1 },
	{ 0x38004000, 0x38007fff, risaf_ahbram2, 0x0, 0xff, 0xff, 1 },
	{ 0x70000000, 0x7fffffff, risaf_xspi2, 0x0, 0xff, 0xff, 1 },
	{ 0x80000000, 0x8fffffff, risaf_xspi3, 0x0, 0xff, 0xff, 1 },
	{ 0x90000000, 0x9fffffff, risaf_xspi1, 0x0, 0xff, 0xff, 1 },
};


/* This function configures RISAF modules to allow unprivileged or privileged, secure-only, read and write access
 * from all masters to all memories. */
int _stm32_risaf_init(void)
{
	unsigned int i;
	int region;
	for (i = 0; i < sizeof(risaf_defConfig) / sizeof(risaf_defConfig[0]); i++) {
		region = _stm32_risaf_getFirstDisabledRegion(risaf_defConfig[i].risaf);
		if (region < 0) {
			return -ENOMEM;
		}

		_stm32_risaf_configRegion(
				risaf_defConfig[i].risaf,
				region,
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
