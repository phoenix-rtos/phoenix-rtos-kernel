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


#define BSEC_BASE      ((void *)0x56009000u)
#define FUSE_MIN       0u
#define FUSE_MID_MIN   128u
#define FUSE_UPPER_MIN 256u
#define FUSE_MAX       375u

#define OTPSR_BUSY      (1u)
#define OTPSR_INIT_DONE (1u << 1)
#define OTPSR_HIDEUP    (1u << 2)
#define OTPSR_OTPNVIR   (1u << 4)
#define OTPSR_OTPERR    (1u << 5)
#define OTPSR_OTPSEC    (1u << 6)
#define OTPSR_PROGFAIL  (1u << 16)
#define OTPSR_DISTURB   (1u << 17)
#define OTPSR_DEDF      (1u << 18)
#define OTPSR_SECF      (1u << 19)
#define OTPSR_PPLF      (1u << 20)
#define OTPSR_PPLMF     (1u << 21)
#define OTPSR_AMEF      (1u << 22)

#define OTPCR_ADDR   (0x1FF)
#define OTPCR_PROG   (1u << 13)
#define OTPCR_PPLOCK (1u << 14)


static struct {
	volatile u32 *base;
} bsec_common;


static void _stm32_bsec_otp_waitBusy(void)
{
	/* Wait until not busy */
	while ((*(bsec_common.base + bsec_otpsr) & OTPSR_BUSY) != 0) {
		;
	}
}


static int _stm32_bsec_otp_checkError(void)
{
	u32 t = *(bsec_common.base + bsec_otpsr);
	if ((t & OTPSR_OTPERR) == 0) {
		return EOK;
	}

	return -EIO;
}


int _stm32_bsec_otp_checkFuseValid(unsigned int fuse)
{
	u32 fuseMax = FUSE_MAX;
	if ((*(bsec_common.base + bsec_otpsr) & OTPSR_HIDEUP) != 0) {
		fuseMax = FUSE_UPPER_MIN - 1;
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
		lockFuse = ~0u;
	}

	/* Program the word using cr register. Fuse word is locked if it's mid or upper */
	t = *(bsec_common.base + bsec_otpcr) & ~(OTPCR_ADDR | OTPCR_PROG | OTPCR_PPLOCK);
	*(bsec_common.base + bsec_otpcr) |= fuse | OTPCR_PROG | (lockFuse & OTPCR_PPLOCK);

	_stm32_bsec_otp_waitBusy();

	t = *(bsec_common.base + bsec_otpsr);
	if ((t & OTPSR_PROGFAIL) != 0) {
		return -EAGAIN;
	}

	if ((t & OTPSR_PPLF) != 0) {
		return -EPERM;
	}

	if ((t & OTPSR_PPLMF) != 0) {
		return -EINVAL;
	}

	/* Reload the fuse word */
	t = *(bsec_common.base + bsec_otpcr) & ~(OTPCR_ADDR | OTPCR_PROG | OTPCR_PPLOCK);
	*(bsec_common.base + bsec_otpcr) = t | fuse;

	_stm32_bsec_otp_waitBusy();

	if ((*(bsec_common.base + bsec_otpsr) & OTPSR_OTPERR) != 0) {
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

	_stm32_rccSetDevClock(pctl_bsec, 1, 1);

	/* Wait until not busy and BSEC initialized */
	do {
		t = *(bsec_common.base + bsec_otpsr);
	} while (((t & OTPSR_BUSY) != 0) || ((t & OTPSR_INIT_DONE) == 0));
}
