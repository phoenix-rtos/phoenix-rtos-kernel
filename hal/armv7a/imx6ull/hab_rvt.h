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

#define HAB_TAG_IVT 0xd1 /* Image Vector Table */
#define HAB_TAG_DCD 0xd2 /* Device Configuration Data */
#define HAB_TAG_CSF 0xd4 /* Command Sequence File */
#define HAB_TAG_CRT 0xd7 /* Certificate */
#define HAB_TAG_SIG 0xd8 /* Signature */
#define HAB_TAG_EVT 0xdb /* Event */
#define HAB_TAG_RVT 0xdd /* ROM Vector Table */
#define HAB_TAG_WRP 0x81 /* Wrapped Key */
#define HAB_TAG_MAC 0xac /* Message Authentication Code */

#define HAB_MAJOR_VERSION 0x04 /* Major version of this HAB */

#define HAB_CMD_SET     0xb1 /* Set */
#define HAB_CMD_INS_KEY 0xbe /* Install Key */
#define HAB_CMD_AUT_DAT 0xca /* Authenticate Data */
#define HAB_CMD_WRT_DAT 0xcc /* Write Data */
#define HAB_CMD_CHK_DAT 0xcf /* Check Data */
#define HAB_CMD_NOP     0xc0 /* No Operation */
#define HAB_CMD_INIT    0xb4 /* Initialize */
#define HAB_CMD_UNLK    0xb2 /* Unlock */

#define HAB_PCL_SRK  0x03 /* SRK certificate format */
#define HAB_PCL_X509 0x09 /* X.509v3 certificate format */
#define HAB_PCL_CMS  0xc5 /* CMS/PKCS#7 signature format */
#define HAB_PCL_BLOB 0xbb /* SHW-specific wrapped key format */
#define HAB_PCL_AEAD 0xa3 /* Proprietary AEAD MAC format */

#define HAB_ALG_ANY    0x00 /* Algorithm type ANY */
#define HAB_ALG_HASH   0x01 /* Hash algorithm type */
#define HAB_ALG_SIG    0x02 /* Signature algorithm type */
#define HAB_ALG_F      0x03 /* Finite field arithmetic */
#define HAB_ALG_EC     0x04 /* Elliptic curve arithmetic */
#define HAB_ALG_CIPHER 0x05 /* Cipher algorithm type */
#define HAB_ALG_MODE   0x06 /* Cipher/hash modes */
#define HAB_ALG_WRAP   0x07 /* Key wrap algorithm type */
#define HAB_ALG_SHA1   0x11 /* SHA-1 algorithm ID */
#define HAB_ALG_SHA256 0x17 /* SHA-256 algorithm ID */
#define HAB_ALG_SHA512 0x1b /* SHA-512 algorithm ID */
#define HAB_ALG_PKCS1  0x21 /* PKCS#1 RSA signature */
#define HAB_ALG_AES    0x55 /* AES algorithm ID */
#define HAB_MODE_CCM   0x66 /* Counter with CBC-MAC */
#define HAB_ALG_BLOB   0x71 /* SHW-specific key wrap */

#define HAB_ENG_ANY    0x00 /* First compatible engine will be selected */
#define HAB_ENG_SCC    0x03 /* Security controller */
#define HAB_ENG_RTIC   0x05 /* Run-time integrity checker */
#define HAB_ENG_SAHARA 0x06 /* Crypto accelerator */
#define HAB_ENG_CSU    0x0a /* Central Security Unit */
#define HAB_ENG_SRTC   0x0c /* Secure clock */
#define HAB_ENG_DCP    0x1b /* Data Co-Processor */
#define HAB_ENG_CAAM   0x1d /* Cryptographic Acceleration and Assurance */
#define HAB_ENG_SNVS   0x1e /* Secure Non-Volatile Storage */
#define HAB_ENG_OCOTP  0x21 /* Fuse controller */
#define HAB_ENG_DTCP   0x22 /* DTCP co-processor */
#define HAB_ENG_ROM    0x36 /* Protected ROM area */
#define HAB_ENG_HDCP   0x24 /* HDCP co-processor */
#define HAB_ENG_SW     0xff /* Software engine */

#define HAB_RTIC_KEEP       0x80 /* Retain reference hash value for */
#define HAB_CAAM_UNLOCK_MID 0x1  /* Leave Job Ring and DECO master */
#define HAB_CAAM_INIT_RNG   0x2  /* Instantiate RNG state handle 0, */

#define HAB_RSN_ANY         0x00 /* Match any reason in hab_rvt.report_event() */
#define HAB_ENG_FAIL        0x30 /* Engine failure */
#define HAB_INV_ADDRESS     0x22 /* Invalid address: access denied */
#define HAB_INV_ASSERTION   0x0c /* Invalid assertion */
#define HAB_INV_CALL        0x28 /* Function called out of sequence */
#define HAB_INV_CERTIFICATE 0x21 /* Invalid certificate */
#define HAB_INV_COMMAND     0x06 /* Invalid command: command malformed */
#define HAB_INV_CSF         0x11 /* Invalid Command Sequence File */
#define HAB_INV_DCD         0x27 /* Invalid Device Configuration Data. */
#define HAB_INV_INDEX       0x0f /* Invalid index: access denied */
#define HAB_INV_IVT         0x05 /* Invalid Image Vector Table */
#define HAB_INV_KEY         0x1d /* Invalid key */
#define HAB_INV_RETURN      0x1e /* Failed callback function */
#define HAB_INV_SIGNATURE   0x18 /* Invalid signature */
#define HAB_INV_SIZE        0x17 /* Invalid data size */
#define HAB_MEM_FAIL        0x2e /* Memory failure */
#define HAB_OVR_COUNT       0x2b /* Expired poll count */
#define HAB_OVR_STORAGE     0x2d /* Exhausted storage region */
#define HAB_UNS_ALGORITHM   0x12 /* Unsupported algorithm */
#define HAB_UNS_COMMAND     0x03 /* Unsupported command */
#define HAB_UNS_ENGINE      0x0a /* Unsupported engine */
#define HAB_UNS_ITEM        0x24 /* Unsupported configuration item */
#define HAB_UNS_KEY         0x1b /* Unsupported key type or parameters */
#define HAB_UNS_PROTOCOL    0x14 /* Unsupported protocol */
#define HAB_UNS_STATE       0x09 /* Unsuitable state */

#define HAB_CTX_ANY          0x00 /* Match any context in */
#define HAB_CTX_ENTRY        0xe1 /* Event logged in hab_rvt.entry() */
#define HAB_CTX_TARGET       0x33 /* Event logged in hab_rvt.check_target() */
#define HAB_CTX_AUTHENTICATE 0x0a /* Event logged in */
#define HAB_CTX_DCD          0xdd /* Event logged in hab_rvt.run_dcd() */
#define HAB_CTX_CSF          0xcf /* Event logged in hab_rvt.run_csf() */
#define HAB_CTX_COMMAND      0xc0 /* Event logged executing Command */
#define HAB_CTX_AUT_DAT      0xdb /* Authenticated data block */
#define HAB_CTX_ASSERT       0xa0 /* Event logged in hab_rvt.assert() */
#define HAB_CTX_EXIT         0xee /* Event logged in hab_rvt.exit() */

#define HAB_CFG_RETURN 0x33 /* Field Return IC */
#define HAB_CFG_OPEN   0xf0 /* Non-secure IC */
#define HAB_CFG_CLOSED 0xcc /* Secure IC */

#define HAB_STS_ANY         0x00 /* Match any status in hab_rvt.report_event(). */
#define HAB_FAILURE         0x33 /* Operation failed */
#define HAB_WARNING         0x69 /* Operation completed with warning */
#define HAB_SUCCESS         0xf0 /* Operation completed successfully */
#define HAB_STATE_INITIAL   0x33 /* Initializing state (transitory) */
#define HAB_STATE_CHECK     0x55 /* Check state (non-secure) */
#define HAB_STATE_NONSECURE 0x66 /* Non-secure state */
#define HAB_STATE_TRUSTED   0x99 /* Trusted state */
#define HAB_STATE_SECURE    0xaa /* Secure state */
#define HAB_STATE_FAIL_SOFT 0xcc /* Soft fail state */
#define HAB_STATE_FAIL_HARD 0xff /* Hard fail state (terminal). */
#define HAB_STATE_NONE      0xf0 /* No security state machine */

#define HAB_TGT_MEMORY     0x0f /* Check memory white list */
#define HAB_TGT_PERIPHERAL 0xf0 /* Check peripheral white list */
#define HAB_TGT_ANY        0x55 /* Check memory & peripheral white list */

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
} imx6ull_hab_rvt_t;

#endif /* _HAB_RVT_H_ */
