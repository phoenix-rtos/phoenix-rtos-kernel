/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * SBI routines (RISCV64)
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "sbi.h"

/* Base extension */
#define SBI_EXT_BASE 0x10

#define SBI_BASE_SPEC_VER      0x0
#define SBI_BASE_IMPL_ID       0x1
#define SBI_BASE_IMPL_VER      0x2
#define SBI_BASE_PROBE_EXT     0x3
#define SBI_BASE_GET_MVENDORID 0x4
#define SBI_BASE_GET_MARCHID   0x5
#define SBI_BASE_GET_MIMPLID   0x6

/* Timer extension */
#define SBI_EXT_TIME      0x54494d45
#define SBI_TIME_SETTIMER 0x0

/* System reset extension */
#define SBI_EXT_SRST   0x53525354
#define SBI_SRST_RESET 0x0

/* IPI extension */
#define SBI_EXT_IPI  0x735049
#define SBI_IPI_SEND 0x0

/* HSM extension */
#define SBI_EXT_HSM     0x48534d
#define SBI_HSM_START   0x0
#define SBI_HSM_STOP    0x1
#define SBI_HSM_STATUS  0x2
#define SBI_HSM_SUSPEND 0x3

/* RFENCE extension */
#define SBI_EXT_RFENCE           0x52464e43
#define SBI_RFNC_I               0x0
#define SBI_RFNC_SFENCE_VMA      0x1
#define SBI_RFNC_SFENCE_VMA_ASID 0x2

/* Legacy extensions */
#define SBI_LEGACY_SETTIMER               0x0
#define SBI_LEGACY_PUTCHAR                0x1
#define SBI_LEGACY_GETCHAR                0x2
#define SBI_LEGACY_CLEARIPI               0x3
#define SBI_LEGACY_SENDIPI                0x4
#define SBI_LEGACY_REMOTE_FENCE_I         0x5
#define SBI_LEGACY_REMOTE_SFENCE_VMA      0x6
#define SBI_LEGACY_REMOTE_SFENCE_VMA_ASID 0x7
#define SBI_LEGACY_SHUTDOWN               0x8

/* clang-format off */
#define SBI_MINOR(x) ((x) & 0xffffff)
/* clang-format on */
#define SBI_MAJOR(x) ((x) >> 24)


static struct {
	u32 specVersion;
	void (*setTimer)(u64);
} sbi_common;


static sbiret_t hal_sbiEcall(int ext, int fid, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	sbiret_t ret;

	register u64 a0 asm("a0") = arg0;
	register u64 a1 asm("a1") = arg1;
	register u64 a2 asm("a2") = arg2;
	register u64 a3 asm("a3") = arg3;
	register u64 a4 asm("a4") = arg4;
	register u64 a5 asm("a5") = arg5;
	register u64 a6 asm("a6") = fid;
	register u64 a7 asm("a7") = ext;

	/* clang-format off */
	__asm__ volatile (
		"ecall"
		: "+r" (a0), "+r" (a1)
		: "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		: "memory");
	/* clang-format on */

	ret.error = a0;
	ret.value = a1;

	return ret;
}

/* Legacy SBI v0.1 calls */

static void hal_sbiSetTimerv01(u64 stime)
{
	(void)hal_sbiEcall(SBI_LEGACY_SETTIMER, 0, stime, 0, 0, 0, 0, 0);
}


long hal_sbiPutchar(int ch)
{
	return hal_sbiEcall(SBI_LEGACY_PUTCHAR, 0, ch, 0, 0, 0, 0, 0).error;
}


long hal_sbiGetchar(void)
{
	return hal_sbiEcall(SBI_LEGACY_GETCHAR, 0, 0, 0, 0, 0, 0, 0).error;
}

/* SBI v0.2+ calls */

sbiret_t hal_sbiGetSpecVersion(void)
{
	return hal_sbiEcall(SBI_EXT_BASE, SBI_BASE_SPEC_VER, 0, 0, 0, 0, 0, 0);
}


sbiret_t hal_sbiProbeExtension(long extid)
{
	return hal_sbiEcall(SBI_EXT_BASE, SBI_BASE_PROBE_EXT, extid, 0, 0, 0, 0, 0);
}


static void hal_sbiSetTimerv02(u64 stime)
{
	(void)hal_sbiEcall(SBI_EXT_TIME, SBI_TIME_SETTIMER, stime, 0, 0, 0, 0, 0);
}


void hal_sbiSetTimer(u64 stime)
{
	sbi_common.setTimer(stime);
}


void hal_sbiReset(u32 type, u32 reason)
{
	if (hal_sbiProbeExtension(SBI_EXT_SRST).error == SBI_SUCCESS) {
		(void)hal_sbiEcall(SBI_EXT_SRST, SBI_SRST_RESET, type, reason, 0, 0, 0, 0);
	}
	__builtin_unreachable();
}


sbiret_t hal_sbiSendIPI(unsigned long hart_mask, unsigned long hart_mask_base)
{
	return hal_sbiEcall(SBI_EXT_IPI, SBI_IPI_SEND, hart_mask, hart_mask_base, 0, 0, 0, 0);
}


sbiret_t hal_sbiHartGetStatus(unsigned long hartid)
{
	return hal_sbiEcall(SBI_EXT_HSM, SBI_HSM_STATUS, hartid, 0, 0, 0, 0, 0);
}


sbiret_t hal_sbiHartStart(unsigned long hartid, unsigned long start_addr, unsigned long opaque)
{
	return hal_sbiEcall(SBI_EXT_HSM, SBI_HSM_START, hartid, start_addr, opaque, 0, 0, 0);
}


void hal_sbiRfenceI(unsigned long hart_mask, unsigned long hart_mask_base)
{
	hal_sbiEcall(SBI_EXT_RFENCE, SBI_RFNC_I, hart_mask, hart_mask_base, 0, 0, 0, 0);
}


sbiret_t hal_sbiSfenceVma(unsigned long hart_mask, unsigned long hart_mask_base, unsigned long vaddr, unsigned long size)
{
	return hal_sbiEcall(SBI_EXT_RFENCE, SBI_RFNC_SFENCE_VMA, hart_mask, hart_mask_base, vaddr, size, 0, 0);
}


sbiret_t hal_sbiSfenceVmaAsid(unsigned long hart_mask, unsigned long hart_mask_base, unsigned long vaddr, unsigned long size, unsigned long asid)
{
	return hal_sbiEcall(SBI_EXT_RFENCE, SBI_RFNC_SFENCE_VMA_ASID, hart_mask, hart_mask_base, vaddr, size, asid, 0);
}


void _hal_sbiInit(void)
{
	sbiret_t ret = hal_sbiGetSpecVersion();
	sbi_common.specVersion = ret.value;

	ret = hal_sbiProbeExtension(SBI_EXT_TIME);
	if (ret.error == SBI_SUCCESS) {
		sbi_common.setTimer = hal_sbiSetTimerv02;
	}
	else {
		sbi_common.setTimer = hal_sbiSetTimerv01;
	}
}
