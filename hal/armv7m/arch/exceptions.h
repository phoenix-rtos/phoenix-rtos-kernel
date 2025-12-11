
#ifndef _PH_HAL_ARMV7M_EXCEPTIONS_H_
#define _PH_HAL_ARMV7M_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 3

#define SIZE_CTXDUMP 320 /* Size of dumped context */


typedef cpu_context_t exc_context_t;

#endif
