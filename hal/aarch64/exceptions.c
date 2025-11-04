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
#define EXTENSION_DESCRIPTIONS 0
#define N_EXCEPTIONS           64

struct {
	void (*handler[N_EXCEPTIONS])(unsigned int, exc_context_t *);
	void (*defaultHandler)(unsigned int, exc_context_t *);
	spinlock_t lock;
} exceptions;


static void exceptions_trampoline(unsigned int n, exc_context_t *ctx)
{
	exceptions.defaultHandler(n, ctx);
}


static const char *exceptionClassStr(unsigned int excClass)
{
	switch (excClass) {
		case 0b000000:
			return "Unknown reason";
		case 0b000001:
			return "Trapped WFI/WFE";
		case 0b000011:
			return "Trapped MCR/MRC access (cp15)";
		case 0b000100:
			return "Trapped MCRR/MRRC access (cp15)";
		case 0b000101:
			return "Trapped MCR/MRC access (cp14)";
		case 0b000110:
			return "Trapped LDC/STC access";
		case 0b000111:
			return "Trapped SME, SVE, Advanced SIMD or floating-point functionality due to CPACR_ELx.FPEN";
		case 0b001100:
			return "Trapped MRRC access (cp14)";
		case 0b001110:
			return "Illegal Execution state";
		case 0b010001:
			return "SVC (AA32)";
		case 0b010100:
			return "Trapped MSRR/MRRS/SYS (AA64)";
		case 0b010101:
			return "SVC (AA64)";
		case 0b011000:
			return "Trapped MSR/MRS/SYS (AA64)";
		case 0b100000:
			return "Instruction Abort (EL0)";
		case 0b100001:
			return "Instruction Abort (EL1)";
		case 0b100010:
			return "PC alignment fault";
		case 0b100100:
			return "Data Abort (EL0)";
		case 0b100101:
			return "Data Abort (EL1)";
		case 0b100110:
			return "SP alignment fault";
		case 0b101000:
			return "Trapped floating-point exception (AA32)";
		case 0b101100:
			return "Trapped floating-point exception (AA64)";
		case 0b101111:
			return "SError exception";
		case 0b110000:
			return "Breakpoint (EL0)";
		case 0b110001:
			return "Breakpoint (EL1)";
		case 0b110010:
			return "Software Step (EL0)";
		case 0b110011:
			return "Software Step (EL1)";
		case 0b110100:
			return "Watchpoint (EL0)";
		case 0b110101:
			return "Watchpoint (EL1)";
		case 0b111000:
			return "BKPT (AA32)";
		case 0b111100:
			return "BRK (AA64)";
#if EXTENSION_DESCRIPTIONS
		case 0b001010:
			return "(FEAT_LS64) Trapped execution of an LD64B or ST64B* instruction";
		case 0b001101:
			return "(FEAT_BTI) Branch Target Exception";
		case 0b011001:
			return "(FEAT_SVE) Access to SVE functionality trapped";
		case 0b011011:
			return "(FEAT_TME) Exception from an access to a TSTART instruction...";
		case 0b011100:
			return "(FEAT_FPAC) Exception from a PAC Fail";
		case 0b011101:
			return "(FEAT_SME) Access to SME functionality trapped";
		case 0b100111:
			return "(FEAT_MOPS) Memory Operation Exception";
		case 0b101101:
			return "(FEAT_GCS) GCS exception";
		case 0b111101:
			return "(FEAT_EBEP) PMU exception";
#endif
		default:
			return "Reserved";
	}
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	size_t i = 0, j;
	const char *toAdd;

	toAdd = "\nException #";
	hal_strcpy(&buff[i], toAdd);
	i += hal_strlen(toAdd);
	buff[i++] = '0' + n / 10;
	buff[i++] = '0' + n % 10;
	buff[i++] = ':';
	buff[i++] = ' ';
	toAdd = exceptionClassStr(n);
	hal_strcpy(&buff[i], toAdd);
	i += hal_strlen(toAdd);

	char prefix[6] = "    =";
	for (j = 0; j < 29; j++) {
		prefix[0] = ((j % 4) == 0) ? '\n' : ' ';
		if (j < 10) {
			prefix[1] = ' ';
			prefix[2] = 'x';
		}
		else {
			prefix[1] = 'x';
			prefix[2] = '0' + (j / 10);
		}

		prefix[3] = '0' + (j % 10);
		i += hal_i2s(prefix, &buff[i], ctx->cpuCtx.x[j], 16, 1);
	}

	i += hal_i2s("  fp=", &buff[i], ctx->cpuCtx.x[29], 16, 1);
	i += hal_i2s("  lr=", &buff[i], ctx->cpuCtx.x[30], 16, 1);
	i += hal_i2s("  sp=", &buff[i], ctx->cpuCtx.sp, 16, 1);

	i += hal_i2s("\npsr=", &buff[i], ctx->cpuCtx.psr, 16, 1);
	i += hal_i2s("  pc=", &buff[i], ctx->cpuCtx.pc, 16, 1);
	i += hal_i2s(" esr=", &buff[i], ctx->esr, 16, 1);
	i += hal_i2s(" far=", &buff[i], ctx->far, 16, 1);

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
#endif

	for (;;) {
		hal_cpuHalt();
	}
}


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


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


unsigned int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	unsigned int prot = 0;
	u32 iss;

	switch (n) {
#ifdef __TARGET_AARCH64A53
		case EXC_SERROR:
			/* Some SError exceptions can result from writing to an invalid address */
			iss = ctx->esr & ((1 << 25) - 1);
			if ((iss & (1 << 24)) == 0) {
				return PROT_NONE;
			}

			iss = (iss & 0x3) | ((iss >> 20) & 0xc);
			prot |= (iss == 0b0010) ? PROT_WRITE : 0; /* SLVERR */
			prot |= (iss == 0b0000) ? PROT_WRITE : 0; /* DECERR */
			return prot;
#endif
		case EXC_INSTR_ABORT_EL0:
			prot |= PROT_USER;
			/* Fall-through */

		case EXC_INSTR_ABORT_EL1:
			prot |= PROT_EXEC | PROT_READ;
			return prot;

		case EXC_DATA_ABORT_EL0:
			prot |= PROT_USER;
			/* Fall-through */

		case EXC_DATA_ABORT_EL1:
			iss = ctx->esr & ((1 << 25) - 1);
			prot |= ((iss & (1 << 6)) == 0) ? PROT_READ : PROT_WRITE;
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
	u32 iss;

	switch (n) {
		case EXC_INSTR_ABORT_EL0: /* Fall-through */
		case EXC_INSTR_ABORT_EL1: /* Fall-through */
		case EXC_DATA_ABORT_EL0:  /* Fall-through */
		case EXC_DATA_ABORT_EL1:
			iss = ctx->esr & ((1 << 25) - 1);
			return ((iss & (1 << 10)) == 0) ? (void *)ctx->far : NULL;

		default:
			return NULL;
	}
}


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
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
	int i;
	hal_spinlockCreate(&exceptions.lock, "exceptions.lock");

	exceptions.defaultHandler = exceptions_defaultHandler;
	for (i = 0; i < N_EXCEPTIONS; i++) {
		exceptions.handler[i] = exceptions_trampoline;
	}
}
