/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Assembly macros
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _ASM_MACROS_H_
#define _ASM_MACROS_H_

#define STR(x)  #x
#define XSTR(x) STR(x)

/* RISC-V registers */
#define REG_ZERO 0
#define REG_RA   1
#define REG_SP   2
#define REG_GP   3
#define REG_TP   4
#define REG_T0   5
#define REG_T1   6
#define REG_T2   7
#define REG_S0   8
#define REG_S1   9
#define REG_A0   10
#define REG_A1   11
#define REG_A2   12
#define REG_A3   13
#define REG_A4   14
#define REG_A5   15
#define REG_A6   16
#define REG_A7   17
#define REG_S2   18
#define REG_S3   19
#define REG_S4   20
#define REG_S5   21
#define REG_S6   22
#define REG_S7   23
#define REG_S8   24
#define REG_S9   25
#define REG_S10  26
#define REG_S11  27
#define REG_T3   28
#define REG_T4   29
#define REG_T5   30
#define REG_T6   31


#define OPCODE_MISC_MEM 0xf
#define FUNCT3_CBO      (0x2 << 12)

/* clang-format off */

/* Instructions supported from GCC 14 */
#define CBO_INVAL(REG) \
	.word ((REG & 0x1f) << 15) | FUNCT3_CBO | OPCODE_MISC_MEM

#define CBO_CLEAN(REG) \
	.word (1 << 20) | ((REG & 0x1f) << 15) | FUNCT3_CBO | OPCODE_MISC_MEM

#define CBO_FLUSH(REG) \
	.word (1 << 21) | ((REG & 0x1f) << 15) | FUNCT3_CBO | OPCODE_MISC_MEM


/* clang-format on */

#endif
