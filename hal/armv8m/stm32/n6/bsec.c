/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * BSEC (Boot and security control) peripheral driver.
 *
 * Copyright 2025 Phoenix Systems
 * Author: Krzysztof Radzewicz, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* On this platform OTP operations (including reads) are only permitted for secure, privileged software.
 * For this reason we cannot move this code to userspace. */

#include "hal/armv8m/stm32/stm32.h"
#include "stm32n6_regs.h"
#include "include/errno.h"
#include "hal/cpu.h"


#define BSEC_BASE      ((void *)0x56009000U)
#define FUSE_MIN       0U
#define FUSE_MID_MIN   128U
#define FUSE_UPPER_MIN 256U
#define FUSE_MAX       375U

#define OTPSR_BUSY      (1U)
#define OTPSR_INIT_DONE (1U << 1)
#define OTPSR_HIDEUP    (1U << 2)
#define OTPSR_OTPNVIR   (1U << 4)
#define OTPSR_OTPERR    (1U << 5)
#define OTPSR_OTPSEC    (1U << 6)
#define OTPSR_PROGFAIL  (1UL << 16)
#define OTPSR_DISTURB   (1UL << 17)
#define OTPSR_DEDF      (1UL << 18)
#define OTPSR_SECF      (1UL << 19)
#define OTPSR_PPLF      (1UL << 20)
#define OTPSR_PPLMF     (1UL << 21)
#define OTPSR_AMEF      (1UL << 22)

#define OTPCR_ADDR   (0x1ffU)
#define OTPCR_PROG   (1UL << 13)
#define OTPCR_PPLOCK (1UL << 14)


static struct {
	volatile u32 *base;
} bsec_common;


static void _stm32_bsec_otp_waitBusy(void)
{
	/* Wait until not busy */
	while ((*(bsec_common.base + bsec_otpsr) & OTPSR_BUSY) != 0U) { }
}


static int _stm32_bsec_otp_checkError(void)
{
	u32 t = *(bsec_common.base + bsec_otpsr);
	if ((t & OTPSR_OTPERR) == 0U) {
		return EOK;
	}

	return -EIO;
}


int _stm32_bsec_otp_checkFuseValid(unsigned int fuse)
{
	u32 fuseMax = FUSE_MAX;
	if ((*(bsec_common.base + bsec_otpsr) & OTPSR_HIDEUP) != 0U) {
		fuseMax = FUSE_UPPER_MIN - 1U;
	}

	if ((fuse >= FUSE_MIN) && (fuse <= fuseMax)) {
		return EOK;
	}

	return -ERANGE;
}


int _stm32_bsec_otp_read(unsigned int fuse, u32 *val)
{
	unsigned int t;

	int res = _stm32_bsec_otp_checkFuseValid(fuse);
	if (res != EOK) {
		return res;
	}

	_stm32_bsec_otp_waitBusy();

	/* Set fuse address */
	t = *(bsec_common.base + bsec_otpcr) & ~(OTPCR_ADDR | OTPCR_PROG | OTPCR_PPLOCK);
	*(bsec_common.base + bsec_otpcr) = t | fuse;

	_stm32_bsec_otp_waitBusy();

	res = _stm32_bsec_otp_checkError();
	if (res != EOK) {
		return res;
	}

	/* Read the reloaded fuse */
	*val = *(bsec_common.base + bsec_fvr0 + fuse);

	return EOK;
}


int _stm32_bsec_otp_write(unsigned int fuse, u32 val)
{
	unsigned int t, lockFuse = 0;

	int res = _stm32_bsec_otp_checkFuseValid(fuse);
	if (res != EOK) {
		return res;
	}

	_stm32_bsec_otp_waitBusy();

	/* Set the word to program */
	*(bsec_common.base + bsec_wdr) = val;

	hal_cpuDataMemoryBarrier();
	if (fuse >= FUSE_MID_MIN) {
		lockFuse = ~0U;
	}

	/* Program the word using cr register. Fuse word is locked if it's mid or upper */
	t = *(bsec_common.base + bsec_otpcr) & ~(OTPCR_ADDR | OTPCR_PROG | OTPCR_PPLOCK);
	*(bsec_common.base + bsec_otpcr) = t | fuse | OTPCR_PROG | (lockFuse & OTPCR_PPLOCK);

	_stm32_bsec_otp_waitBusy();

	t = *(bsec_common.base + bsec_otpsr);
	if ((t & OTPSR_PROGFAIL) != 0U) {
		return -EAGAIN;
	}

	if ((t & OTPSR_PPLF) != 0U) {
		return -EPERM;
	}

	if ((t & OTPSR_PPLMF) != 0U) {
		return -EINVAL;
	}

	/* Reload the fuse word */
	t = *(bsec_common.base + bsec_otpcr) & ~(OTPCR_ADDR | OTPCR_PROG | OTPCR_PPLOCK);
	*(bsec_common.base + bsec_otpcr) = t | fuse;

	_stm32_bsec_otp_waitBusy();

	if ((*(bsec_common.base + bsec_otpsr) & OTPSR_OTPERR) != 0U) {
		return -EAGAIN;
	}

	/* Compare the loaded word to val*/
	if (*(bsec_common.base + bsec_fvr0 + fuse) != val) {
		return -EAGAIN;
	}

	return EOK;
}


void _stm32_bsec_init(void)
{
	u32 t;
	bsec_common.base = BSEC_BASE;

	(void)_stm32_rccSetDevClock(pctl_bsec, 1U, 1U);

	/* Wait until not busy and BSEC initialized */
	do {
		t = *(bsec_common.base + bsec_otpsr);
	} while (((t & OTPSR_BUSY) != 0U) || ((t & OTPSR_INIT_DONE) == 0U));
}
