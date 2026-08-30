 /*
 * Copyright 2025 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* Specify output format */
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

/* Entry point */
ENTRY(_start)

SECTIONS
{
	. = ORIGIN(KERNEL_IMAGE);
	.init : 
	{
		__init_start = .;
		KEEP (*(SORT_NONE(.init)))
		__init_end = .;
	} > KERNEL_IMAGE

    .gnu.sgstubs : { *(.gnu.sgstubs*) }  

	.text : 
	{
		__text_start = .;
		*(SORT(.text.sorted.*))
		*(.text .stub .text.* .gnu.linkonce.t.*)
		*(.glue_7)      /* glue code to call Thumb functions from ARM mode */
		*(.glue_7t)     /* glue code to call ARM functions from Thumb mode */
		*(.eh_frame)
        *(.vfp11_veneer)
		PROVIDE_HIDDEN(__text_end = .); /* use __etext to include .fini */
	} > KERNEL_IMAGE

	.fini : { KEEP (*(SORT_NONE(.fini))) } > KERNEL_IMAGE

	/* define a global symbol at the end of the code */
	PROVIDE (__etext = .);
	PROVIDE (_etext = .);
	PROVIDE (etext = .);

	.rodata : {
		__rodata_load = LOADADDR(.rodata);
		__rodata_start = .;
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		__rodata_end = .;
	} > KERNEL_IMAGE

    .rodata1 : { *(.rodata1) }

    .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) }
    
	.data : 
	{
		. = ALIGN(4);
		__data_load = LOADADDR(.data);
		__data_start = .;
		*(.data)
		*(.data*)
		*(.gnu.linkonce.d.*)
		. = ALIGN(4);
		__data_end = .;
		_edata = .;
		PROVIDE (edata = .);
	} > DATA

	.persistent : 
  	{
		. = ALIGN(4);
    	PROVIDE (__persistent_start = .);
    	*(.persistent .persistent.* .gnu.linkonce.p.*)
   		. = ALIGN(4);
    	PROVIDE (__persistent_end = .);
  	}

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		__bss_start = .;
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(4);
		__bss_end = .;

	} > BSS

	.syspage :
	{
		. = ALIGN(4);
		*(.syspage);
	} > SYSPAGE

	.noinit (NOLOAD) :
 	{
		. =  ALIGN(4);
		PROVIDE (__noinit_start = .);
    	*(.noinit .noinit.* .gnu.linkonce.n.*)
    	. = ALIGN(4);
    	PROVIDE (__noinit_end = .);
  	} > DATA

	. = ALIGN(4);
	_end = .;
	PROVIDE (end = .);

	.ARM.attributes 0 : { *(.ARM.attributes) }

	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }
}
