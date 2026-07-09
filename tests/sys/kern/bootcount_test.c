/*-
 * Copyright (c) 2026 Vidith Hundekar
 * All rights reserved.
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

#include <sys/types.h>
#include <sys/sysctl.h>

#include <atf-c.h>
#include <errno.h>
#include <stdint.h>

/*
 * kern.bootcount must be readable and return a uint64_t value.
 */
ATF_TC_WITHOUT_HEAD(bootcount_readable);
ATF_TC_BODY(bootcount_readable, tc)
{
	uint64_t counter;
	size_t len;

	len = sizeof(counter);
	ATF_REQUIRE(sysctlbyname("kern.bootcount", &counter, &len,
	    NULL, 0) == 0);
	ATF_CHECK(len == sizeof(uint64_t));
}

/*
 * kern.bootcount must return the same value on repeated reads
 * within a single boot session.
 */
ATF_TC_WITHOUT_HEAD(bootcount_stable);
ATF_TC_BODY(bootcount_stable, tc)
{
	uint64_t counter1, counter2;
	size_t len;

	len = sizeof(counter1);
	ATF_REQUIRE(sysctlbyname("kern.bootcount",
	    &counter1, &len, NULL, 0) == 0);
	ATF_REQUIRE(sysctlbyname("kern.bootcount",
	    &counter2, &len, NULL, 0) == 0);
	ATF_CHECK(counter1 == counter2);
}

/*
 * kern.bootcount_fw_backed should be readable and returns true on EFI systems.
 */
ATF_TC_WITHOUT_HEAD(bootcount_fw_backed);
ATF_TC_BODY(bootcount_fw_backed, tc)
{
	int val;
	size_t len;

	len = sizeof(val);
	ATF_REQUIRE(sysctlbyname("kern.bootcount_fw_backed",
	    &val, &len, NULL, 0) == 0);
	ATF_CHECK(val == 1);
}

/*
 * kern.bootcount must reject writes with EPERM once locked at boot.
 */
ATF_TC(bootcount_eperm);
ATF_TC_HEAD(bootcount_eperm, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(bootcount_eperm, tc)
{
	uint64_t val;
	size_t len;

	val = 2026;
	len = sizeof(val);
	ATF_REQUIRE_ERRNO(EPERM,
	    sysctlbyname("kern.bootcount", NULL, NULL,
	        &val, len) == -1);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bootcount_readable);
	ATF_TP_ADD_TC(tp, bootcount_stable);
	ATF_TP_ADD_TC(tp, bootcount_fw_backed);
	ATF_TP_ADD_TC(tp, bootcount_eperm);

	return (atf_no_error());
}