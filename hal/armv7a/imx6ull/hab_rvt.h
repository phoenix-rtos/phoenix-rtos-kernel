/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAB ROM vector table definition for i.MX 6ULL
 *
 * Adapted from "High Assurance Boot Version 4 Application Programming Interface Reference Manual" Copyright 2018-2019 NXP
 * Copyright 2026 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#ifndef _HAB_RVT_H_
#define _HAB_RVT_H_

#include "hal/types.h"

#define HAB_ADDR ((void *)0x100U)

typedef u8 hab_status_t;
typedef u32 hab_hdr_t;
typedef u8 hab_target_t;
typedef u8 hab_assertion_t;
typedef u8 hab_config_t;
typedef u8 hab_state_t;
typedef hab_status_t (*hab_loader_callback_f)(void **start, size_t *bytes, const void *boot_data);
typedef void (*hab_image_entry_f)(void);
typedef s32 hab_ptrdiff_t;

typedef struct {
	/* Header with tag HAB_TAG_RVT, length and HAB version fields (see Data Structures) */
	hab_hdr_t hdr;
	/* Enter and initialize HAB library. */
	hab_status_t (*entry)(void);
	/* Finalize and exit HAB library. */
	hab_status_t (*exit)(void);
	/* Check target address. */
	hab_status_t (*check_target)(hab_target_t type, const void *start, size_t bytes);
	/* Authenticate image. */
	hab_image_entry_f (*authenticate_image)(u8 cid, hab_ptrdiff_t ivt_offset, void **start, size_t *bytes, hab_loader_callback_f loader);
	/* Execute a boot configuration script. */
	hab_status_t (*run_dcd)(const u8 *dcd);
	/* Execute an authentication script. */
	hab_status_t (*run_csf)(const u8 *csf, u8 cid);
	/* Test an assertion against the audit log. */
	hab_status_t (*assert)(hab_assertion_t type, const void *data, u32 count);
	/* Report an event from the audit log. */
	hab_status_t (*report_event)(hab_status_t status, u32 index, u8 *event, size_t *bytes);
	/* Report security status. */
	hab_status_t (*report_status)(hab_config_t *config, hab_state_t *state);
	/* Enter failsafe boot mode. */
	void (*failsafe)(void);
	/* Authenticate image. */
	hab_image_entry_f (*authenticate_image_no_dcd)(u8 cid, hab_ptrdiff_t ivt_offset, void **start, size_t *bytes, hab_loader_callback_f loader);
	/* Get HAB version. */
	u32 (*get_version)(void);
	/* Authenticate container. */
	hab_status_t (*authenticate_container)(u8 cid, hab_ptrdiff_t ivt_offset, void **start, size_t *bytes, hab_loader_callback_f loader, u32 srkmask, int skip_dcd);
} hab_rvt_t;


#endif /* _HAB_RVT_H_ */
