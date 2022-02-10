/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System information page (prepared by kernel loader)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_MULTIBOOT_H_
#define _HAL_MULTIBOOT_H_

#define MULTIBOOT_HDR_MAGIC            0x1BADB002  /* The magic field should contain this. */
#define MULTIBOOT_HDR_PAGEALIGN        0x00000001  /* Align all boot modules on i386 page (4KB) boundaries. */
#define MULTIBOOT_HDR_MEMINFO          0x00000002  /* Must pass memory information to OS. */
#define MULTIBOOT_HDR_VIDEOMODE        0x00000004  /* Must pass video information to OS. */
#define MULTIBOOT_HDR_AOUT             0x00010000  /* This flag indicates the use of the address fields in the header. */

#define MULTIBOOT_INFO_MAGIC           0x2BADB002  /* This should be in %eax. */
#define MULTIBOOT_INFO_MEMORY          0x00000001  /* is there basic lower/upper memory information? */
#define MULTIBOOT_INFO_BOOTDEV         0x00000002  /* is there a boot device set? */
#define MULTIBOOT_INFO_CMDLINE         0x00000004  /* is the command-line defined? */
#define MULTIBOOT_INFO_MODS            0x00000008  /* are there modules to do something with? */
#define MULTIBOOT_INFO_AOUTSYMS        0x00000010  /* is there a symbol table loaded? */
#define MULTIBOOT_INFO_ELFSHDR         0X00000020  /* is there an ELF section header table? */
#define MULTIBOOT_INFO_MEMMAP          0x00000040  /* is there a full memory map? */
#define MULTIBOOT_INFO_DRIVEINFO       0x00000080  /* Is there drive info? */
#define MULTIBOOT_INFO_CONFIGTABLE     0x00000100  /* Is there a config table? */
#define MULTIBOOT_INFO_BOOTLOADER      0x00000200  /* Is there a boot loader name? */
#define MULTIBOOT_INFO_APM             0x00000400  /* Is there a APM table? */
#define MULTIBOOT_INFO_VBE             0x00000800  /* Is there video information? */
#define MULTIBOOT_INFO_FRAMEBUFFER     0x00001000


#ifndef __ASSEMBLY__

#include "arch/types.h"

typedef struct {
	u32 flags;

	u32 mem_lower;           /* Available memory from BIOS */
	u32 mem_upper;

	u32 boot_device;         /* "root" partition */
	u32 cmdline;             /* Kernel command line */

	u32 mods_count;          /* Boot-Module list */
	u32 mods_addr;
	u32 syms[4];

	u32 mmap_length;         /* Memory Mapping buffer */
	u32 mmap_addr;

	u32 drives_length;       /* Drive Info buffer */
	u32 drives_addr;

	u32 config_table;        /* ROM configuration table */
	u32 boot_loader_name;    /* Boot Loader Name */
	u32 apm_table;           /* APM table */

	u32 vbe_control_info;    /* Video */
	u32 vbe_mode_info;
	u16 vbe_mode;
	u16 vbe_interface_seg;
	u16 vbe_interface_off;
	u16 vbe_interface_len;

	u64 framebuffer_addr;
	u32 framebuffer_pitch;
	u32 framebuffer_width;
	u32 framebuffer_height;
	u8 framebuffer_bpp;
	u8 framebuffer_type;
	union
	{
		struct
		{
			u32 framebuffer_palette_addr;
			u16 framebuffer_palette_num_colors;
		};
		struct
		{
			u8 framebuffer_red_field_position;
			u8 framebuffer_red_mask_size;
			u8 framebuffer_green_field_position;
			u8 framebuffer_green_mask_size;
			u8 framebuffer_blue_field_position;
			u8 framebuffer_blue_mask_size;
		};
	};
} multiboot_info_t;


enum { fbIndexed = 0, fbRGB = 1, fbText = 2 };


typedef struct {
	u32 size;
	u64 addr;
	u64 len;
	u32 type;
} __attribute__((packed)) multiboot_mmitem_t;


enum { memAvail = 1, memReserved = 2, memACPI = 3, memNVS = 4, memBad = 5 };


typedef struct {
	u32 mod_start;
	u32 mod_end;
	u32 cmdline;
	u32 pad;
} multiboot_mod_t;


typedef struct {
	u16 version;
	u16 cseg;
	u32 offset;
	u16 cseg_16;
	u16 dseg;
	u16 flags;
	u16 cseg_len;
	u16 cseg_16_len;
	u16 dseg_len;
} multiboot_apm_t;

#endif

#endif
