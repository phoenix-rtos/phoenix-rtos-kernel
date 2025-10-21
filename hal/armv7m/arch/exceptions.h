
#ifndef _PH_HAL_ARMV7M_EXCEPTIONS_H_
#define _PH_HAL_ARMV7M_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128U

#define EXC_UNDEFINED 3

#define SIZE_CTXDUMP 320 /* Size of dumped context */


typedef struct _exc_context_t {
	/* Saved by ISR */
	u32 psp;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;
	u32 r11;
	u32 excret;

	/* Saved by hardware */
	cpu_hwContext_t mspctx;
} exc_context_t;

#endif
