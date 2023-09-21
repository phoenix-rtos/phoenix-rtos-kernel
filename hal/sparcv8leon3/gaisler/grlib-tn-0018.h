/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * GRLIB-TN-0018 Errata
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifdef LEON3_TN_0018_FIX


/* LEON3 Cache controller register - ASI 2 */
#define ASI_CCTRL    0x2
#define CCTRL_IP_BIT 15  /* ICache flush pending bit */
#define CCTRL_ICS    0x3 /* ICache state */

/* clang-format off */

#define TN_0018_WAIT_ICACHE(out1, out2) \
1: \
	/* Wait for ICache flush to complete */; \
	lda [%g0] ASI_CCTRL, out1; \
	srl out1, CCTRL_IP_BIT, out2; \
	andcc out2, 1, %g0; \
	bne 1b; \
	andn out1, CCTRL_ICS, out2


#define TN_0018_FIX(in1, in2) \
.align 0x20                  /* Align sta for performance */; \
	sta in2, [%g0] ASI_CCTRL /* Disable ICache */; \
	nop                      /* Delay */; \
	or %l1, %l1, %l1         /* Delay + catch rf parity error on l1 */; \
	or %l2, %l2, %l2         /* Delay + catch rf parity error on l2 */; \
	sta in1, [%g0] ASI_CCTRL /* Re-enable ICache after rett */; \
	nop                      /* Delay ensures insn after gets cached */


#else


#define TN_0018_WAIT_ICACHE(out1, out2)

#define TN_0018_FIX(in1, in2)

/* clang-format on */

#endif
