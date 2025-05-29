
#ifndef _HAL_ARMV7M_EXCEPTIONS_H_
#define _HAL_ARMV7M_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 3

#define SIZE_CTXDUMP          320 /* Size of dumped context */
#define SIZE_COREDUMP_GREGSET 72
#ifdef CPU_IMXRT
#define SIZE_COREDUMP_THREADAUX 280 /* vfp context note */
#define SIZE_COREDUMP_GENAUX    36  /* auxv HWCAP note */
#else
#define SIZE_COREDUMP_THREADAUX 0
#define SIZE_COREDUMP_GENAUX    0
#endif

#define HAL_ELF_MACHINE 40 /* ARM */

typedef cpu_context_t exc_context_t;

#endif
