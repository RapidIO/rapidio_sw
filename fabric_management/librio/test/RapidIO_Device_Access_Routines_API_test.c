/*
 ************************************************************************
 Copyright (c) 2017, Integrated Device Technology Inc.
 Copyright (c) 2017, RapidIO Trade Association
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 l of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this l of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <stdarg.h>
#include <setjmp.h>
#include "cmocka.h"

#include "RapidIO_Source_Config.h"
#include "RapidIO_Device_Access_Routines_API.h"
#include "rio_ecosystem.h"

#ifdef __cplusplus
extern "C" {
#endif

// No mocking of read/write required
uint32_t __real_DARRegRead(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t *readdata);
uint32_t __real_DARRegWrite(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t writedata);

uint32_t __wrap_DARRegRead(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t *readdata)
{
	return __real_DARRegRead(dev_info, offset, readdata);
}

uint32_t __wrap_DARRegWrite(DAR_DEV_INFO_t *dev_info, uint32_t offset,
		uint32_t writedata)
{
	return __real_DARRegWrite(dev_info, offset, writedata);
}

static void assumptions(void **state)
{
	assert_int_equal(0x00, RIO_UNITIALIZED_DEVICE);
	assert_int_equal(0x01, RIO_CPS_DEVICE);
	assert_int_equal(0x02, RIO_RXS_DEVICE);
	assert_int_equal(0x04, RIO_TSI721_DEVICE);
	assert_int_equal(0x08, RIO_TSI57X_DEVICE);
	assert_int_equal(0x10, RIO_UNKNOWN_DEVICE);

	(void)state; // unused
}

static void rio_get_driver_family_test(void **state)
{
	rio_driver_family_t expected;

	// obvious wrong values
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0));
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0xffffffff));

	// CPS
#ifdef CPS_DAR_WANTED
	expected = RIO_CPS_DEVICE;
#else
	expected = RIO_UNKNOWN_DEVICE;
#endif
	assert_int_equal(expected, rio_get_driver_family(0x03740038)); // CPS1848
	assert_int_equal(expected, rio_get_driver_family(0x03750038)); // CPS1432
	assert_int_equal(expected, rio_get_driver_family(0x03790038)); // CPS1616
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x03770038)); // VPS1616
	assert_int_equal(expected, rio_get_driver_family(0x03780038)); // SPS1848

	// RXS
#ifdef RXSx_DAR_WANTED
	expected = RIO_RXS_DEVICE;
#else
	expected = RIO_UNKNOWN_DEVICE;
#endif
	assert_int_equal(expected, rio_get_driver_family(0x80e60038)); // RXS2448
	assert_int_equal(expected, rio_get_driver_family(0x80e50038)); // RXS1632
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x80e00038)); // RXSx

	// TSI721
#ifdef TSI721_DAR_WANTED
	expected = RIO_TSI721_DEVICE;
#else
	expected = RIO_UNKNOWN_DEVICE;
#endif
	assert_int_equal(expected, rio_get_driver_family(0x80ab0038)); // TSI721

	// TSI5X
#ifdef TSI57X_DAR_WANTED
	expected = RIO_TSI57X_DEVICE;
#else
	expected = RIO_UNKNOWN_DEVICE;
#endif
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0568000d)); // TSI500
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0568000d)); // TSI568
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0570000d)); // TSI570
	assert_int_equal(expected, rio_get_driver_family(0x0572000d)); // TSI572
	assert_int_equal(expected, rio_get_driver_family(0x0574000d)); // TSI574
	assert_int_equal(expected, rio_get_driver_family(0x0578000d)); // TSI576
	assert_int_equal(expected, rio_get_driver_family(0x0577000d)); // TSI577
	assert_int_equal(expected, rio_get_driver_family(0x0578000d)); // TSI578

	// CPS with wrong vendor code (Tundra for example)
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0374000d)); // CPS1848
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0375000d)); // CPS1432
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0379000d)); // CPS1616
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0377000d)); // VPS1616
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x0378000d)); // SPS1848

	// RXS with wrong vendor code (Tundra for example)
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x80e6000d)); // RXS2448
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x80e5000d)); // RXS1632
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x80e0000d)); // RXSx

	// TSI721 with wrong vendor code (Tundra for example)
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x80ab000d)); // TSI721

	// TSI5X with wrong vendor code (IDT for example)
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05680038)); // TSI500
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05680038)); // TSI568
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05700038)); // TSI570
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05720038)); // TSI572
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05740038)); // TSI574
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05780038)); // TSI578
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05770038)); // TSI577
	assert_int_equal(RIO_UNKNOWN_DEVICE, rio_get_driver_family(0x05780038)); // TSI578

	(void)state; // unused
}

int main(int argc, char** argv)
{
	(void)argv; // not used
	argc++; // not used

	const struct CMUnitTest tests[] = {
	cmocka_unit_test(assumptions),
	cmocka_unit_test(rio_get_driver_family_test),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

#ifdef __cplusplus
}
#endif
