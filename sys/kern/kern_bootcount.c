/*-
 * Copyright (c) 2026 Bruce Simpson.
 * Copyright (c) 2026 Vidith Hundekar.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/efi.h>
#include <sys/linker.h>
#include <sys/boot.h>
#if defined(__amd64__) || defined(__i386__)
#include <x86/metadata.h>
#elif defined(__aarch64__) || defined(__riscv)
#include <machine/metadata.h>
#endif
#include <sys/bootcount.h>

/*
 * This file is mostly a placeholder. We cannot actually implement
 * a boot counter owned by the kernel without some help from
 * nonvolatile storage. The EFI specification contains its own
 * boot counter, and recent versions of uBoot also support one.
 *
 * The counter needs to be treated as immutable once set. If a
 * filesystem is used as the nonvolatile backing store, the
 * operation must be read-modify-write atomic. This has the additional
 * problem that the storage used must be visible to the kernel, and of
 * course there are no guarantees regarding which filesystems will
 * be visible at boot, and of course, filesystems will not be read-write
 * until they are actually mounted. ZFS systems may be able to get
 * away with this by setting properties within the default pool.
 *
 * For now, we simply allow read-write access to this counter,
 * assuming that an rc script will take care of everything for us.
 *
 */

static uint64_t kern_efi_bootcnt = 0;        
static bool kern_bootcnt_lock = false;
static bool kern_bootcnt_firmware_backed = false;

static int
sysctl_kern_bootcount(SYSCTL_HANDLER_ARGS)
{
	uint64_t new_counter;
	int error;
	if (req->newptr != NULL) {
		if (kern_bootcnt_lock) {
			return (EPERM);
		}
		error = SYSCTL_IN(req, &new_counter, sizeof(new_counter));
		if (error) {
			return (error);
		}
		kern_efi_bootcnt = new_counter;
		kern_bootcnt_lock = true;
		return (0);
	}
	return (SYSCTL_OUT(req, &kern_efi_bootcnt, sizeof(kern_efi_bootcnt)));
}

SYSCTL_PROC(_kern, OID_AUTO, bootcount,
	CTLTYPE_U64 | CTLFLAG_RW | CTLFLAG_MPSAFE,
	NULL, 0, sysctl_kern_bootcount, "QU",
	"EFI firmware monotonic boot counter");
SYSCTL_BOOL(_kern, OID_AUTO, bootcount_fw_backed,
	CTLFLAG_RD, &kern_bootcnt_firmware_backed, 0,
	"True if kern.bootcount is backed by EFI firmware");

/*
 * Stage 1 (SI_SUB_KMEM): read EFI monotonic counter from loader
 * metadata. If found, lock immediately — no RS call needed.
 */
static void
bootcount_init_bs(void *dummy __unused)
{
	uint64_t bs_counter;
	
	bs_counter = MD_FETCH(preload_kmdp, MODINFOMD_EFI_MTC, uint64_t);
	if (bs_counter == 0) {
		printf("kern_bootcount: BS value not found\n");
		return;
	}
	kern_efi_bootcnt = bs_counter;
	kern_bootcnt_firmware_backed = true;
	kern_bootcnt_lock = true;
	printf("kern_bootcount: BS value = 0x%016llx\n",
		(unsigned long long)bs_counter);
}
SYSINIT(bootcount_bs, SI_SUB_KMEM, SI_ORDER_ANY, bootcount_init_bs, NULL);

/*
 * Stage 2 (SI_SUB_DRIVERS, SI_ORDER_THIRD): runs only if Stage 1
 * found no BS value. Calls RS->GetNextHighMonotonicCount() once
 * and uses (rs_high << 32) as the boot epoch. Low 32 bits are
 * unknown after reset so they are set to zero.
 */
static void 
bootcount_init_rs(void *dummy __unused)
{
	uint32_t high_cnt_rt;
	int error;
	
	if (kern_bootcnt_lock)
		return;

	error = efi_get_next_high_monotone(&high_cnt_rt);
	if (error != 0) {
		printf("kern_bootcount: RS call failed (%d), "
			"awaiting rc script fallback\n", error);
		return;
	}

	kern_efi_bootcnt = (uint64_t)high_cnt_rt << 32;
	kern_bootcnt_firmware_backed = true;
	kern_bootcnt_lock = true;
	printf("kern_bootcount: RS fallback value = 0x%016llx\n",
	    (unsigned long long)kern_efi_bootcnt);
}

SYSINIT(bootcount_rs, SI_SUB_DRIVERS, SI_ORDER_THIRD, bootcount_init_rs, NULL);

uint64_t
kern_get_bootcount(void)
{
	return (kern_efi_bootcnt);
}

bool
kern_bootcount_is_fw_backed(void)
{
    return (kern_bootcnt_firmware_backed);
}