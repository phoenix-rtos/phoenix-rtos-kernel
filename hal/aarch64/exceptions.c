/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/exceptions.h"
#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/console.h"
#include "hal/string.h"
#include "include/mman.h"


/* Set to 1 to print text descriptions of exceptions for architecture extensions */
#define EXTENSION_DESCRIPTIONS 0U
#define N_EXCEPTIONS           64U

static struct {
	excHandlerFn_t handler[N_EXCEPTIONS];
	excHandlerFn_t defaultHandler;
	spinlock_t lock;
} exceptions;


static void exceptions_trampoline(unsigned int n, exc_context_t *ctx)
{
	exceptions.defaultHandler(n, ctx);
}


static const char *exceptionClassStr(unsigned int excClass)
{
	switch (excClass) {
		case 0:
			return "Unknown reason";
		case 1:
			return "Trapped WFI/WFE";
		case 3:
			return "Trapped MCR/MRC access (cp15)";
		case 4:
			return "Trapped MCRR/MRRC access (cp15)";
		case 5:
			return "Trapped MCR/MRC access (cp14)";
		case 6:
			return "Trapped LDC/STC access";
		case 7:
			return "Trapped SME, SVE, Advanced SIMD or floating-point functionality due to CPACR_ELx.FPEN";
		case 12:
			return "Trapped MRRC access (cp14)";
		case 14:
			return "Illegal Execution state";
		case 17:
			return "SVC (AA32)";
		case 20:
			return "Trapped MSRR/MRRS/SYS (AA64)";
		case 21:
			return "SVC (AA64)";
		case 24:
			return "Trapped MSR/MRS/SYS (AA64)";
		case 32:
			return "Instruction Abort (EL0)";
		case 33:
			return "Instruction Abort (EL1)";
		case 34:
			return "PC alignment fault";
		case 36:
			return "Data Abort (EL0)";
		case 37:
			return "Data Abort (EL1)";
		case 38:
			return "SP alignment fault";
		case 40:
			return "Trapped floating-point exception (AA32)";
		case 44:
			return "Trapped floating-point exception (AA64)";
		case 47:
			return "SError exception";
		case 48:
			return "Breakpoint (EL0)";
		case 49:
			return "Breakpoint (EL1)";
		case 50:
			return "Software Step (EL0)";
		case 51:
			return "Software Step (EL1)";
		case 52:
			return "Watchpoint (EL0)";
		case 53:
			return "Watchpoint (EL1)";
		case 56:
			return "BKPT (AA32)";
		case 60:
			return "BRK (AA64)";
#if EXTENSION_DESCRIPTIONS
		case 10:
			return "(FEAT_LS64) Trapped execution of an LD64B or ST64B* instruction";
		case 13:
			return "(FEAT_BTI) Branch Target Exception";
		case 25:
			return "(FEAT_SVE) Access to SVE functionality trapped";
		case 27:
			return "(FEAT_TME) Exception from an access to a TSTART instruction...";
		case 28:
			return "(FEAT_FPAC) Exception from a PAC Fail";
		case 29:
			return "(FEAT_SME) Access to SME functionality trapped";
		case 39:
			return "(FEAT_MOPS) Memory Operation Exception";
		case 45:
			return "(FEAT_GCS) GCS exception";
		case 61:
			return "(FEAT_EBEP) PMU exception";
#endif
		default:
			return "Reserved";
	}
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	size_t i = 0;
	u8 j;
	const char *toAdd;

	toAdd = "\nException #";
	(void)hal_strcpy(&buff[i], toAdd);
	i += hal_strlen(toAdd);
	buff[i++] = '0' + n / 10U;
	buff[i++] = '0' + n % 10U;
	buff[i++] = ':';
	buff[i++] = ' ';
	toAdd = exceptionClassStr(n);
	(void)hal_strcpy(&buff[i], toAdd);
	i += hal_strlen(toAdd);

	char prefix[6] = "    =";
	for (j = 0; j < 29U; j++) {
		prefix[0] = ((j % 4U) == 0U) ? '\n' : ' ';
		if (j < 10U) {
			prefix[1] = ' ';
			prefix[2] = 'x';
		}
		else {
			prefix[1] = 'x';
			prefix[2] = (char)('0' + (j / 10U));
		}

		prefix[3] = (char)('0' + (j % 10U));
		i += hal_i2s(prefix, &buff[i], ctx->cpuCtx.x[j], 16U, 1U);
	}

	i += hal_i2s("  fp=", &buff[i], ctx->cpuCtx.x[29], 16U, 1U);
	i += hal_i2s("  lr=", &buff[i], ctx->cpuCtx.x[30], 16U, 1U);
	i += hal_i2s("  sp=", &buff[i], ctx->cpuCtx.sp, 16U, 1U);

	i += hal_i2s("\npsr=", &buff[i], ctx->cpuCtx.psr, 16U, 1U);
	i += hal_i2s("  pc=", &buff[i], ctx->cpuCtx.pc, 16U, 1U);
	i += hal_i2s(" esr=", &buff[i], ctx->esr, 16U, 1U);
	i += hal_i2s(" far=", &buff[i], ctx->far, 16U, 1U);

	buff[i++] = '\n';
	buff[i] = '\0';
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#else
	for (;;) {
		hal_cpuHalt();
	}
#endif
}


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Usage in assembly" */
void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	if (n >= N_EXCEPTIONS) {
		return;
	}

	exceptions.handler[n](n, ctx);
	/* Handle signals if necessary */
	if (hal_cpuSupervisorMode(&ctx->cpuCtx) == 0) {
		threads_setupUserReturn((void *)ctx->cpuCtx.x[0], &ctx->cpuCtx);
	}
}


vm_prot_t hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	vm_prot_t prot = 0;
	u32 iss;

	switch (n) {
#ifdef __TARGET_AARCH64A53
		case EXC_SERROR:
			/* Some SError exceptions can result from writing to an invalid address */
			iss = (u32)(ctx->esr & ((1UL << 25) - 1U));
			if ((iss & (1UL << 24)) == 0U) {
				return PROT_NONE;
			}

			iss = (iss & 0x3U) | ((iss >> 20) & 0xcU);
			prot |= (iss == 2U) ? PROT_WRITE : 0U; /* SLVERR */
			prot |= (iss == 0U) ? PROT_WRITE : 0U; /* DECERR */
			return prot;
#endif
		/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
		case EXC_INSTR_ABORT_EL0:
			prot |= PROT_USER;
			/* Fall-through */

		case EXC_INSTR_ABORT_EL1:
			prot |= PROT_EXEC | PROT_READ;
			return prot;

		/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
		case EXC_DATA_ABORT_EL0:
			prot |= PROT_USER;
			/* Fall-through */

		case EXC_DATA_ABORT_EL1:
			iss = (u32)(ctx->esr & ((1UL << 25) - 1U));
			prot |= ((iss & (1UL << 6)) == 0U) ? PROT_READ : PROT_WRITE;
			return prot;

		default:
			return PROT_NONE;
	}
}

ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->cpuCtx.pc;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	unsigned long iss;

	switch (n) {
		/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
		case EXC_INSTR_ABORT_EL0: /* Fall-through */
		/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
		case EXC_INSTR_ABORT_EL1: /* Fall-through */
		/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
		case EXC_DATA_ABORT_EL0: /* Fall-through */
		case EXC_DATA_ABORT_EL1:
			iss = ctx->esr & ((1UL << 25) - 1U);
			return ((iss & (1UL << 10)) == 0U) ? (void *)ctx->far : NULL;

		default:
			return NULL;
	}
}


int hal_exceptionsSetHandler(unsigned int n, excHandlerFn_t handler)
{
	int ret = 0;
	spinlock_ctx_t sc;

	hal_spinlockSet(&exceptions.lock, &sc);
	if (n < N_EXCEPTIONS) {
		exceptions.handler[n] = handler;
	}
	else if (n == EXC_DEFAULT) {
		exceptions.defaultHandler = handler;
	}
	else if (n == EXC_PAGEFAULT) {
		exceptions.handler[EXC_INSTR_ABORT_EL0] = handler;
		exceptions.handler[EXC_INSTR_ABORT_EL1] = handler;
		exceptions.handler[EXC_DATA_ABORT_EL0] = handler;
		exceptions.handler[EXC_DATA_ABORT_EL1] = handler;
	}
	else {
		ret = -1;
	}

	hal_spinlockClear(&exceptions.lock, &sc);

	return ret;
}


void _hal_exceptionsInit(void)
{
	unsigned int i;
	hal_spinlockCreate(&exceptions.lock, "exceptions.lock");

	exceptions.defaultHandler = exceptions_defaultHandler;
	for (i = 0; i < N_EXCEPTIONS; i++) {
		exceptions.handler[i] = exceptions_trampoline;
	}
}
