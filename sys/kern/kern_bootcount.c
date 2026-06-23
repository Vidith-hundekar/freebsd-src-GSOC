/*-
 * Copyright (c) 2014 Bruce Simpson.
 * Copyright (c) 2026 Vidith Hundekar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_bootcount.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/efi.h>         
#include <sys/linker.h>      
#include <sys/boot.h>       
#include <machine/metadata.h>
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
static uint32_t kern_bs_high = 0;        
static uint32_t kern_bs_low  = 0;       
static bool kern_bootcnt_lock = false;
static bool kern_bootcnt_firmware_backed = false;

static bool
is_high32_newer(uint32_t new_high, uint32_t old_high)
{
     static const uint32_t uint32_max = UINT32_MAX ;
     return  (new_high > old_high && new_high - old_high <= uint32_max/2) ||(old_high > new_high && old_high - new_high > uint32_max/2);
}

static int
sysctl_kern_bootcount(SYSCTL_HANDLER_ARGS)
{
	uint64_t new_counter;
	int error;
	if (req->newptr != NULL) {
		if (kern_bootcnt_lock) {
			return EPERM;
		}
		error = SYSCTL_IN(req, &new_counter, sizeof(new_counter));
		if (error) {
			return error;
		}
		kern_efi_bootcnt = new_counter;
		kern_bootcnt_lock = true;
		return 0;
	}
	return SYSCTL_OUT(req, &kern_efi_bootcnt,sizeof(kern_efi_bootcnt));
}

SYSCTL_PROC(_kern, OID_AUTO, bootcount, CTLTYPE_U64 | CTLFLAG_RW | CTLFLAG_MPSAFE,NULL, 0, sysctl_kern_bootcount, "QU","EFI firmware monotonic boot counter");


static void
bootcount_init_bs(void *dummy __unused)
{
	uint64_t bs_counter = MD_FETCH(preload_kmdp, MODINFOMD_EFI_MTC, uint64_t);
	if (bs_counter == 0) {
		printf("kern_bootcount: MODINFOMD_EFI_MTC not found\n");
		return;
	}
     	kern_bs_high = bs_counter >> 32;
     	kern_bs_low  = bs_counter & 0xFFFFFFFF;
     	kern_efi_bootcnt = bs_counter;
	printf("kern_bootcount: BS value = 0x%016llx\n", (unsigned long long)bs_counter);
}
SYSINIT(bootcount_bs, SI_SUB_KMEM, SI_ORDER_ANY, bootcount_init_bs, NULL);

static void 
bootcount_init_rs(void *dummy __unused)
{
	uint32_t high_cnt_rt;
	int error;
	
	/* especially for Non-UEFI platforms */
	if (kern_bs_high == 0 && kern_bs_low == 0) {
		printf("kern_bootcount: no BS value found, skipping RS call\n");
		return;
	}

	kern_bootcnt_firmware_backed = true;
	error = efi_get_next_high_monotone(&high_cnt_rt);
	if (error != 0) {
		printf("kern_bootcount: RS call failed (%d), using BS value as-is\n", error);
		goto lock;
	}

	if (is_high32_newer(high_cnt_rt, kern_bs_high)) {
		kern_efi_bootcnt = ((uint64_t)high_cnt_rt << 32) | 0;
		printf("kern_bootcount: high bits updated via RS, final = 0x%016llx\n", (unsigned long long)kern_efi_bootcnt);
	} else {
		printf("kern_bootcount: no wraparound detected, keeping BS value = 0x%016llx\n", (unsigned long long)kern_efi_bootcnt);
	}

lock:
	kern_bootcnt_lock = true;
	printf("kern_bootcount: final value locked = 0x%016llx\n", (unsigned long long)kern_efi_bootcnt);
}

SYSINIT(bootcount_rs, SI_SUB_DRIVERS, SI_ORDER_THIRD,bootcount_init_rs, NULL);

uint64_t
kern_get_bootcount(void)
{
    return kern_efi_bootcnt;
}

bool
kern_bootcount_is_fw_backed(void)
{
    return kern_bootcnt_firmware_backed;
}