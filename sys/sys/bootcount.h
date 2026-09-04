/*-
 * Copyright (c) 2026 Vidith Hundekar.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _SYS_BOOTCOUNT_H_
#define _SYS_BOOTCOUNT_H_

#ifdef _KERNEL

#include <sys/types.h>

/*
 * Public KPI for the EFI-backed persistent boot counter.
 *
 * kern_get_bootcount() returns the UEFI monotonic counter value
 * captured at boot.
 *
 * kern_bootcount_is_fw_backed() returns true if the counter came
 * from EFI firmware. Returns false if kern.bootcount was set by
 * the non-UEFI fallback rc script or is unavailable.
 */
uint64_t kern_get_bootcount(void);
bool     kern_bootcount_is_fw_backed(void);

#endif	/* _KERNEL */

#endif	/* _SYS_BOOTCOUNT_H_ */