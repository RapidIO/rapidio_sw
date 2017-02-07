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
#include "RapidIO_Utilities_API.h"
#include "src/RapidIO_Utilities_API.c"
#include "rio_ecosystem.h"

#ifdef __cplusplus
extern "C" {
#endif

static void assumptions(void **state)
{
	assert_int_equal(276, RIO_MAX_PKT_BYTES);
	assert_int_equal(256, RIO_MAX_PKT_PAYLOAD);

	assert_int_equal(0, stype0_min);
	assert_int_equal(0, stype0_pa);
	assert_int_equal(1, stype0_rty);
	assert_int_equal(2, stype0_pna);
	assert_int_equal(3, stype0_rsvd);
	assert_int_equal(4, stype0_status);
	assert_int_equal(5, stype0_VC_status);
	assert_int_equal(6, stype0_lresp);
	assert_int_equal(7, stype0_imp);
	assert_int_equal(7, stype0_max);

	assert_int_equal(1, PNA_PKT_UNEXP_ACKID);
	assert_int_equal(2, PNA_CS_UNEXP_ACKID);
	assert_int_equal(3, PNA_NONMTC_STOPPED);
	assert_int_equal(4, PNA_PKT_BAD_CRC);
	assert_int_equal(5, PNA_DELIN_ERR);
	assert_int_equal(6, PNA_RETRY);
	assert_int_equal(7, PNA_NO_DESCRAM_SYNC);
	assert_int_equal(0x1F, PNA_GENERAL_ERR);

	assert_int_equal(0, stype1_min);
	assert_int_equal(0, stype1_sop);
	assert_int_equal(1, stype1_stomp);
	assert_int_equal(2, stype1_eop);
	assert_int_equal(3, stype1_rfr);
	assert_int_equal(4, stype1_lreq);
	assert_int_equal(5, stype1_mecs);
	assert_int_equal(6, stype1_rsvd);
	assert_int_equal(7, stype1_nop);
	assert_int_equal(7, stype1_max);

	assert_int_equal(0, STYPE1_CMD_RSVD);
	assert_int_equal(3, STYPE1_LREQ_CMD_RST_DEV);
	assert_int_equal(4, STYPE1_LREQ_CMD_PORT_STAT);

	assert_int_equal(0, stype2_min);
	assert_int_equal(0, stype2_nop);
	assert_int_equal(1, stype2_vc_stat);
	assert_int_equal(2, stype2_rsvd2);
	assert_int_equal(3, stype2_rsvd3);
	assert_int_equal(4, stype2_rsvd4);
	assert_int_equal(5, stype2_rsvd5);
	assert_int_equal(6, stype2_rsvd6);
	assert_int_equal(7, stype2_rsvd7);
	assert_int_equal(7, stype2_max);

	assert_int_equal(0, pkt_done);
	assert_int_equal(3, pkt_retry);
	assert_int_equal(7, pkt_err);

	assert_int_equal(0x78000000, DAR_UTIL_INVALID_TT);
	assert_int_equal(0x78000001, DAR_UTIL_BAD_ADDRSIZE);
	assert_int_equal(0x78000002, DAR_UTIL_INVALID_RDSIZE);
	assert_int_equal(0x78000003, DAR_UTIL_BAD_DATA_SIZE);
	assert_int_equal(0x78000004, DAR_UTIL_INVALID_MTC);
	assert_int_equal(0x78000005, DAR_UTIL_BAD_MSG_DSIZE);
	assert_int_equal(0x78000006, DAR_UTIL_BAD_RESP_DSIZE);
	assert_int_equal(0x78000007, DAR_UTIL_UNKNOWN_TRANS);
	assert_int_equal(0x78000008, DAR_UTIL_BAD_DS_DSIZE);
	assert_int_equal(0x78000009, DAR_UTIL_UNKNOWN_STATUS);
	assert_int_equal(0x78000010, DAR_UTIL_UNKNOWN_FTYPE);
	assert_int_equal(0x78000011, DAR_UTIL_0_MASK_VAL_ERR);
	assert_int_equal(0x78000012, DAR_UTIL_INVALID_WPTR);

	assert_int_equal(0xFFFF, BAD_FTYPE);
	assert_int_equal(0xFFFFFFFF, BAD_SIZE);

	(void)state; // unused
}

static void DAR_util_get_ftype_test(void **state)
{
	assert_int_equal(16, DAR_util_get_ftype(pkt_raw));

	assert_int_equal(2, DAR_util_get_ftype(pkt_nr));
	assert_int_equal(2, DAR_util_get_ftype(pkt_nr_inc));
	assert_int_equal(2, DAR_util_get_ftype(pkt_nr_dec));
	assert_int_equal(2, DAR_util_get_ftype(pkt_nr_set));
	assert_int_equal(2, DAR_util_get_ftype(pkt_nr_clr));

	assert_int_equal(5, DAR_util_get_ftype(pkt_nw));
	assert_int_equal(5, DAR_util_get_ftype(pkt_nwr));
	assert_int_equal(5, DAR_util_get_ftype(pkt_nw_swap));
	assert_int_equal(5, DAR_util_get_ftype(pkt_nw_cmp_swap));
	assert_int_equal(5, DAR_util_get_ftype(pkt_nw_tst_swap));

	assert_int_equal(6, DAR_util_get_ftype(pkt_sw));

	assert_int_equal(7, DAR_util_get_ftype(pkt_fc));

	assert_int_equal(8, DAR_util_get_ftype(pkt_mr));
	assert_int_equal(8, DAR_util_get_ftype(pkt_mw));
	assert_int_equal(8, DAR_util_get_ftype(pkt_mrr));
	assert_int_equal(8, DAR_util_get_ftype(pkt_mwr));
	assert_int_equal(8, DAR_util_get_ftype(pkt_pw));

	assert_int_equal(9, DAR_util_get_ftype(pkt_dstm));

	assert_int_equal(10, DAR_util_get_ftype(pkt_db));

	assert_int_equal(11, DAR_util_get_ftype(pkt_msg));

	assert_int_equal(13, DAR_util_get_ftype(pkt_resp));
	assert_int_equal(13, DAR_util_get_ftype(pkt_resp_data));

	assert_int_equal(13, DAR_util_get_ftype(pkt_msg_resp));

	assert_int_equal(BAD_FTYPE,
			DAR_util_get_ftype((DAR_pkt_type )(pkt_type_max + 1)));

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_1_test(void **state)
{
	const uint32_t pkt_bytes = 1;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(0, rdsize);
			break;
		case 0x1:
		case 0x9:
			assert_int_equal(0, wdptr);
			assert_int_equal(1, rdsize);
			break;

		case 0x2:
		case 0xa:
			assert_int_equal(0, wdptr);
			assert_int_equal(2, rdsize);
			break;
		case 0x3:
		case 0xb:
			assert_int_equal(0, wdptr);
			assert_int_equal(3, rdsize);
			break;
		case 0x4:
		case 0xc:
			assert_int_equal(1, wdptr);
			assert_int_equal(0, rdsize);
			break;
		case 0x5:
		case 0xd:
			assert_int_equal(1, wdptr);
			assert_int_equal(1, rdsize);
			break;
		case 0x6:
		case 0xe:
			assert_int_equal(1, wdptr);
			assert_int_equal(2, rdsize);
			break;
		case 0x7:
		case 0xf:
			assert_int_equal(1, wdptr);
			assert_int_equal(3, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_2_test(void **state)
{
	const uint32_t pkt_bytes = 2;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(4, rdsize);
			break;
		case 0x2:
		case 0xa:
			assert_int_equal(0, wdptr);
			assert_int_equal(6, rdsize);
			break;
		case 0x4:
		case 0xc:
			assert_int_equal(1, wdptr);
			assert_int_equal(4, rdsize);
			break;
		case 0x6:
		case 0xe:
			assert_int_equal(1, wdptr);
			assert_int_equal(6, rdsize);
			break;
		case 0x1:
		case 0x3:
		case 0x5:
		case 0x7:
		case 0x9:
		case 0xb:
		case 0xd:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_3_test(void **state)
{
	const uint32_t pkt_bytes = 3;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(5, rdsize);
			break;
		case 0x5:
		case 0xd:
			assert_int_equal(1, wdptr);
			assert_int_equal(5, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_4_test(void **state)
{
	const uint32_t pkt_bytes = 4;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(8, rdsize);
			break;
		case 0x4:
		case 0xc:
			assert_int_equal(1, wdptr);
			assert_int_equal(8, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_5_test(void **state)
{
	const uint32_t pkt_bytes = 5;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(7, rdsize);
			break;
		case 0x3:
		case 0xb:
			assert_int_equal(1, wdptr);
			assert_int_equal(7, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_6_test(void **state)
{
	const uint32_t pkt_bytes = 6;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(9, rdsize);
			break;
		case 0x2:
		case 0xa:
			assert_int_equal(1, wdptr);
			assert_int_equal(9, rdsize);
			break;
		case 0x1:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_7_test(void **state)
{
	const uint32_t pkt_bytes = 7;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(0xa, rdsize);
			break;
		case 0x1:
		case 0x9:
			assert_int_equal(1, wdptr);
			assert_int_equal(0xa, rdsize);
			break;
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_8_test(void **state)
{
	const uint32_t pkt_bytes = 8;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(0xb, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_16_test(void **state)
{
	const uint32_t pkt_bytes = 16;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(1, wdptr);
			assert_int_equal(0xb, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_32_test(void **state)
{
	const uint32_t pkt_bytes = 32;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(0xc, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_64_test(void **state)
{
	const uint32_t pkt_bytes = 64;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(1, wdptr);
			assert_int_equal(0xc, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_96_test(void **state)
{
	const uint32_t pkt_bytes = 96;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(0xd, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_128_test(void **state)
{
	const uint32_t pkt_bytes = 128;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(1, wdptr);
			assert_int_equal(0xd, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_160_test(void **state)
{
	const uint32_t pkt_bytes = 160;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(0xe, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_192_test(void **state)
{
	const uint32_t pkt_bytes = 192;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(1, wdptr);
			assert_int_equal(0xe, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_224_test(void **state)
{
	const uint32_t pkt_bytes = 224;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0, wdptr);
			assert_int_equal(0xf, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_pkt_bytes_256_test(void **state)
{
	const uint32_t pkt_bytes = 256;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		rdsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize, &wdptr);
		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(1, wdptr);
			assert_int_equal(0xf, rdsize);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_rdsize_wdptr_test(void **state)
{
	uint32_t pkt_bytes;

	uint32_t addr;
	uint32_t rdsize;
	uint32_t wdptr;
	int idx;

	wdptr = 0xbeef;
	for (pkt_bytes = 0; pkt_bytes < 1024; pkt_bytes++) {
		for (idx = 0; idx < 0x10; idx++) {
			addr = 0xcafebab0 + idx;
			rdsize = 0xdead;

			switch (pkt_bytes) {
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 16:
			case 32:
			case 64:
			case 96:
			case 128:
			case 160:
			case 192:
			case 224:
			case 256:
				// handled above
				continue;
			default:
				break;
			}

			DAR_util_get_rdsize_wdptr(addr, pkt_bytes, &rdsize,
					&wdptr);
			assert_int_equal(BAD_SIZE, wdptr);
			assert_int_equal(BAD_SIZE, rdsize);
		}
	}

	(void)state; // unused
}

static void DAR_util_compute_rd_bytes_n_align_wptr_0_test(void **state)
{
	const uint32_t wptr = 0;

	uint32_t num_bytes;
	uint32_t align;
	uint32_t rc;

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(0, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(1, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(1, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(2, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(2, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(3, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(3, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(4, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(5, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(3, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(6, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(2, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(7, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(5, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(8, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(4, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(9, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(6, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(10, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(7, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(11, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(8, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(12, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x20, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(13, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x60, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(14, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0xa0, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(15, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0xe0, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(16, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	(void)state; // not used
}

static void DAR_util_compute_rd_bytes_n_align_wptr_1_test(void **state)
{
	const uint32_t wptr = 1;

	uint32_t num_bytes;
	uint32_t align;
	uint32_t rc;

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(0, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(4, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(1, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(5, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(2, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(6, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(3, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(7, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(4, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(4, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(5, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(3, num_bytes);
	assert_int_equal(5, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(6, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(6, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(7, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(5, num_bytes);
	assert_int_equal(3, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(8, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(4, num_bytes);
	assert_int_equal(4, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(9, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(6, num_bytes);
	assert_int_equal(2, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(10, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(7, num_bytes);
	assert_int_equal(1, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(11, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x10, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(12, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x40, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(13, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x80, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(14, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0xc0, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(15, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x100, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_rd_bytes_n_align(16, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	(void)state; // not used
}

static void DAR_util_compute_rd_bytes_n_align_test(void **state)
{
	uint32_t wptr;
	uint32_t num_bytes;
	uint32_t align;
	uint32_t rc;

	for (wptr = 2; wptr < 0x100; wptr++) {
		rc = DAR_util_compute_rd_bytes_n_align(9, wptr, &num_bytes,
				&align);
		assert_int_equal(SIZE_RC_FAIL, rc);
		assert_int_equal(0, num_bytes);
		assert_int_equal(0, align);
	}

	(void)state; // not used
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_0_test(void **state)
{
	const uint32_t pkt_bytes = 0;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);
		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_1_test(void **state)
{
	const uint32_t pkt_bytes = 1;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
			assert_int_equal(idx % 4, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(idx % 4, wrsize);
			assert_int_equal(1, wdptr);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_2_test(void **state)
{
	const uint32_t pkt_bytes = 2;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(4, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x2:
		case 0xa:
			assert_int_equal(6, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x4:
		case 0xc:
			assert_int_equal(4, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x6:
		case 0xe:
			assert_int_equal(6, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x1:
		case 0x3:
		case 0x5:
		case 0x7:
		case 0x9:
		case 0xb:
		case 0xd:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_3_test(void **state)
{
	const uint32_t pkt_bytes = 3;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(5, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x5:
		case 0xd:
			assert_int_equal(5, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_4_test(void **state)
{
	const uint32_t pkt_bytes = 4;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(8, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x4:
		case 0xc:
			assert_int_equal(8, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_5_test(void **state)
{
	const uint32_t pkt_bytes = 5;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(7, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x3:
		case 0xb:
			assert_int_equal(7, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x1:
		case 0x2:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_6_test(void **state)
{
	const uint32_t pkt_bytes = 6;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(9, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x2:
		case 0xa:
			assert_int_equal(9, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x1:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;

		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_7_test(void **state)
{
	const uint32_t pkt_bytes = 7;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0xa, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x1:
		case 0x9:
			assert_int_equal(0xa, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;

		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_8_test(void **state)
{
	const uint32_t pkt_bytes = 8;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0xb, wrsize);
			assert_int_equal(0, wdptr);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_9_test(void **state)
{
	const uint32_t pkt_bytes = 9;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_10_test(void **state)
{
	const uint32_t pkt_bytes = 10;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_11_test(void **state)
{
	const uint32_t pkt_bytes = 11;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_12_test(void **state)
{
	const uint32_t pkt_bytes = 12;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_13_test(void **state)
{
	const uint32_t pkt_bytes = 13;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_14_test(void **state)
{
	const uint32_t pkt_bytes = 14;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_15_test(void **state)
{
	const uint32_t pkt_bytes = 15;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		assert_int_equal(BAD_SIZE, wrsize);
		assert_int_equal(BAD_SIZE, wdptr);
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_pkt_bytes_16_test(void **state)
{
	const uint32_t pkt_bytes = 16;

	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (idx = 0; idx < 0x10; idx++) {
		addr = 0xcafebab0 + idx;
		wrsize = 0xdead;
		wdptr = 0xbeef;

		DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize, &wdptr);

		switch (idx) {
		case 0x0:
		case 0x8:
			assert_int_equal(0xb, wrsize);
			assert_int_equal(1, wdptr);
			break;
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
		case 0xf:
			assert_int_equal(BAD_SIZE, wrsize);
			assert_int_equal(BAD_SIZE, wdptr);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void DAR_util_get_wrsize_wdptr_test(void **state)
{
	uint32_t pkt_bytes;
	uint32_t addr;
	uint32_t wrsize;
	uint32_t wdptr;
	int idx;

	// the least significant digit of the address is used to determine
	// the return values, so cycle through 0-F with a random value in the
	// most significant digits

	for (pkt_bytes = 17; pkt_bytes < 257; pkt_bytes++) {
		for (idx = 0; idx < 0x10; idx++) {
			addr = 0xcafebab0 + idx;
			wrsize = 0xdead;
			wdptr = 0xbeef;

			DAR_util_get_wrsize_wdptr(addr, pkt_bytes, &wrsize,
					&wdptr);

			switch (pkt_bytes) {
			case 0x18:
			case 0x20:
				if ((0 == idx) || (8 == idx)) {
					assert_int_equal(0xc, wrsize);
					assert_int_equal(0, wdptr);
				} else {
					assert_int_equal(BAD_SIZE, wrsize);
					assert_int_equal(BAD_SIZE, wdptr);
				}
				break;
			case 0x28:
			case 0x30:
			case 0x38:
			case 0x40:
				if ((0 == idx) || (8 == idx)) {
					assert_int_equal(0xc, wrsize);
					assert_int_equal(1, wdptr);
				} else {
					assert_int_equal(BAD_SIZE, wrsize);
					assert_int_equal(BAD_SIZE, wdptr);
				}
				break;
			case 0x48:
			case 0x50:
			case 0x58:
			case 0x60:
			case 0x68:
			case 0x70:
			case 0x78:
			case 0x80:
				if ((0 == idx) || (8 == idx)) {
					assert_int_equal(0xd, wrsize);
					assert_int_equal(1, wdptr);
				} else {
					assert_int_equal(BAD_SIZE, wrsize);
					assert_int_equal(BAD_SIZE, wdptr);
				}
				break;
			case 0x88:
			case 0x90:
			case 0x98:
			case 0xa0:
			case 0xa8:
			case 0xb0:
			case 0xb8:
			case 0xc0:
			case 0xc8:
			case 0xd0:
			case 0xd8:
			case 0xe0:
			case 0xe8:
			case 0xf0:
			case 0xf8:
			case 0x100:
				if ((0 == idx) || (8 == idx)) {
					assert_int_equal(0xf, wrsize);
					assert_int_equal(1, wdptr);
				} else {
					assert_int_equal(BAD_SIZE, wrsize);
					assert_int_equal(BAD_SIZE, wdptr);
				}
				break;
			default:
				assert_int_equal(BAD_SIZE, wrsize);
				assert_int_equal(BAD_SIZE, wdptr);
			}
		}
	}

	(void)state; // unused
}

static void DAR_util_compute_wr_bytes_n_align_wptr_0_test(void **state)
{
	const uint32_t wptr = 0;

	uint32_t num_bytes;
	uint32_t align;
	uint32_t rc;

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(0, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(1, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(1, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(2, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(2, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(3, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(3, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(4, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(5, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(3, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(6, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(2, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(7, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(5, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(8, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(4, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(9, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(6, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(10, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(7, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(11, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(8, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(12, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x20, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(13, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(14, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(15, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(16, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	(void)state; // not used
}

static void DAR_util_compute_wr_bytes_n_align_wptr_1_test(void **state)
{
	const uint32_t wptr = 1;

	uint32_t num_bytes;
	uint32_t align;
	uint32_t rc;

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(0, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(4, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(1, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(5, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(2, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(6, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(3, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(1, num_bytes);
	assert_int_equal(7, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(4, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(4, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(5, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(3, num_bytes);
	assert_int_equal(5, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(6, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(2, num_bytes);
	assert_int_equal(6, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(7, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(5, num_bytes);
	assert_int_equal(3, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(8, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(4, num_bytes);
	assert_int_equal(4, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(9, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(6, num_bytes);
	assert_int_equal(2, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(10, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(7, num_bytes);
	assert_int_equal(1, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(11, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x10, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(12, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x40, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(13, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x80, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(14, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(15, wptr, &num_bytes, &align);
	assert_int_equal(0, rc);
	assert_int_equal(0x100, num_bytes);
	assert_int_equal(0, align);

	num_bytes = 0x12345678;
	align = 0xdead;
	rc = DAR_util_compute_wr_bytes_n_align(16, wptr, &num_bytes, &align);
	assert_int_equal(SIZE_RC_FAIL, rc);
	assert_int_equal(0, num_bytes);
	assert_int_equal(0, align);

	(void)state; // not used
}

static void DAR_util_compute_wr_bytes_n_align_test(void **state)
{
	uint32_t wptr;
	uint32_t num_bytes;
	uint32_t align;
	uint32_t rc;

	for (wptr = 2; wptr < 0x100; wptr++) {
		rc = DAR_util_compute_wr_bytes_n_align(9, wptr, &num_bytes,
				&align);
		assert_int_equal(SIZE_RC_FAIL, rc);
		assert_int_equal(0, num_bytes);
		assert_int_equal(0, align);
	}

	(void)state; // not used
}

static void DAR_util_pkt_bytes_init_test(void **state)
{
	DAR_pkt_bytes_t comp_pkt;
	int idx;

	memset(&comp_pkt, 0xa5, sizeof(DAR_pkt_bytes_t));
	assert_int_equal(0xa5a5a5a5, comp_pkt.num_chars);
	for (idx = 0; idx < RIO_MAX_PKT_BYTES; idx++) {
		assert_int_equal(0xa5, comp_pkt.pkt_data[idx]);;
	}

	DAR_util_pkt_bytes_init(&comp_pkt);
	assert_int_equal(0xFFFFFFFF, comp_pkt.num_chars);
	assert_int_equal(rio_addr_34, comp_pkt.pkt_addr_size);
	assert_false(comp_pkt.pkt_has_crc);
	assert_false(comp_pkt.pkt_padded);
	for (idx = 0; idx < RIO_MAX_PKT_BYTES; idx++) {
		assert_int_equal(0, comp_pkt.pkt_data[idx]);;
	}

	memset(&comp_pkt, 0x5a, sizeof(DAR_pkt_bytes_t));
	assert_int_equal(0x5a5a5a5a, comp_pkt.num_chars);
	for (idx = 0; idx < RIO_MAX_PKT_BYTES; idx++) {
		assert_int_equal(0x5a, comp_pkt.pkt_data[idx]);;
	}

	DAR_util_pkt_bytes_init(&comp_pkt);
	assert_int_equal(0xFFFFFFFF, comp_pkt.num_chars);
	assert_int_equal(rio_addr_34, comp_pkt.pkt_addr_size);
	assert_false(comp_pkt.pkt_has_crc);
	assert_false(comp_pkt.pkt_padded);
	for (idx = 0; idx < RIO_MAX_PKT_BYTES; idx++) {
		assert_int_equal(0, comp_pkt.pkt_data[idx]);;
	}

	(void)state; // unused
}

static void tst_init_rw_addr_data(uint8_t *expected, DAR_pkt_bytes_t *bytes_out,
		rio_addr_size pkt_addr_size, uint32_t num_chars_in)
{
	memset(bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	bytes_out->pkt_addr_size = pkt_addr_size;
	bytes_out->num_chars = num_chars_in;
	bytes_out->pkt_has_crc = true;
	memset(bytes_out->pkt_data, 0xa5, num_chars_in);
	memset(expected, 0xa5, num_chars_in);

	switch (pkt_addr_size) {
	case rio_addr_21:
		expected[num_chars_in] = 0xad;
		expected[num_chars_in + 1] = 0xbe;
		expected[num_chars_in + 2] = 0xe8;
		break;
	case rio_addr_32:
		expected[num_chars_in] = 0xde;
		expected[num_chars_in + 1] = 0xad;
		expected[num_chars_in + 2] = 0xbe;
		expected[num_chars_in + 3] = 0xe8;
		break;
	case rio_addr_34:
		expected[num_chars_in] = 0xde;
		expected[num_chars_in + 1] = 0xad;
		expected[num_chars_in + 2] = 0xbe;
		expected[num_chars_in + 3] = 0xea;
		break;
	case rio_addr_50:
		expected[num_chars_in] = 0xba;
		expected[num_chars_in + 1] = 0xbe;
		expected[num_chars_in + 2] = 0xde;
		expected[num_chars_in + 3] = 0xad;
		expected[num_chars_in + 4] = 0xbe;
		expected[num_chars_in + 5] = 0xea;
		break;
	case rio_addr_66:
		expected[num_chars_in] = 0xca;
		expected[num_chars_in + 1] = 0xfe;
		expected[num_chars_in + 2] = 0xba;
		expected[num_chars_in + 3] = 0xbe;
		expected[num_chars_in + 4] = 0xde;
		expected[num_chars_in + 5] = 0xad;
		expected[num_chars_in + 6] = 0xbe;
		expected[num_chars_in + 7] = 0xea;
		break;
	default:
		fail_msg("Invalid %u address size", pkt_addr_size);
	}
}

static void DAR_add_rw_addr_addr_size_21_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_21;
	const uint32_t num_chars_in = 6;
	const uint32_t num_chars_out = 6 + 3;

	uint32_t addr[3] = {0xdeadbeef, 0xcafebabe, 0xa5a55a5a};
	uint32_t wptr;
	DAR_pkt_bytes_t bytes_out;

	uint32_t rc;
	uint8_t expected[RIO_MAX_PKT_BYTES];

	// valid wptr
	wptr = 0;
	tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
			num_chars_in);

	rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
	assert_int_equal(0, rc);
	assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);
	assert_int_equal(num_chars_out, bytes_out.num_chars);
	assert_memory_equal(expected, bytes_out.pkt_data, num_chars_out);

	// valid wptr
	wptr = 1;
	tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
			num_chars_in);
	expected[num_chars_in + 2] = 0xec;

	rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
	assert_int_equal(0, rc);
	assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);
	assert_int_equal(num_chars_out, bytes_out.num_chars);
	assert_memory_equal(expected, bytes_out.pkt_data, num_chars_out);

	// invalid wptr
	tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
			num_chars_in);
	for (wptr = 2; wptr < 10; wptr++) {
		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(DAR_UTIL_INVALID_WPTR, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_in, bytes_out.num_chars);
		assert_memory_equal(expected, bytes_out.pkt_data, num_chars_in);
	}

	(void)state; // unused
}

static void DAR_add_rw_addr_addr_size_32_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_32;
	const uint32_t num_chars_in = 6;
	const uint32_t num_chars_out = 6 + 4;

	uint32_t addr[3] = {0xdeadbeef, 0xcafebabe, 0xa5a55a5a};
	uint32_t wptr;
	DAR_pkt_bytes_t bytes_out;

	uint32_t rc;
	uint8_t expected[RIO_MAX_PKT_BYTES];

	// valid wptr
	for (wptr = 0; wptr < 2; wptr++) {
		tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
				num_chars_in);

		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(0, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_out, bytes_out.num_chars);
		if (0 == wptr) {
			expected[num_chars_out - 1] = 0xe8;
		} else {
			expected[num_chars_out - 1] = 0xec;
		}
		assert_memory_equal(expected, bytes_out.pkt_data,
				num_chars_out);
	}

	// invalid wptr
	tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
			num_chars_in);
	for (wptr = 2; wptr < 10; wptr++) {
		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(DAR_UTIL_INVALID_WPTR, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_in, bytes_out.num_chars);
		assert_memory_equal(expected, bytes_out.pkt_data, num_chars_in);
	}

	(void)state; // unused
}

static void DAR_add_rw_addr_addr_size_34_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_34;
	const uint32_t num_chars_in = 6;
	const uint32_t num_chars_out = 6 + 4;

	uint32_t addr[3] = {0xdeadbeef, 0xcafebabe, 0xa5a55a5a};
	uint32_t wptr;
	DAR_pkt_bytes_t bytes_out;

	uint32_t rc;
	uint8_t expected[RIO_MAX_PKT_BYTES];

	// valid wptr
	for (wptr = 0; wptr < 2; wptr++) {
		tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
				num_chars_in);

		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(0, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_out, bytes_out.num_chars);
		if (0 == wptr) {
			expected[num_chars_out - 1] = 0xea;
		} else {
			expected[num_chars_out - 1] = 0xee;
		}
		assert_memory_equal(expected, bytes_out.pkt_data,
				num_chars_out);
	}

	// invalid wptr
	tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
			num_chars_in);
	for (wptr = 2; wptr < 10; wptr++) {
		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(DAR_UTIL_INVALID_WPTR, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_in, bytes_out.num_chars);
		assert_memory_equal(expected, bytes_out.pkt_data, num_chars_in);
	}

	(void)state; // unused
}

static void DAR_add_rw_addr_addr_size_50_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_50;
	const uint32_t num_chars_in = 6;
	const uint32_t num_chars_out = 6 + 6;

	uint32_t addr[3] = {0xdeadbeef, 0xcafebabe, 0xa5a55a5a};
	uint32_t wptr;
	DAR_pkt_bytes_t bytes_out;

	uint32_t rc;
	uint8_t expected[RIO_MAX_PKT_BYTES];

	// valid wptr
	for (wptr = 0; wptr < 2; wptr++) {
		tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
				num_chars_in);

		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(0, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_out, bytes_out.num_chars);
		if (0 == wptr) {
			expected[num_chars_out - 1] = 0xea;
		} else {
			expected[num_chars_out - 1] = 0xee;
		}
		assert_memory_equal(expected, bytes_out.pkt_data,
				num_chars_out);
	}

	// invalid wptr
	tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
			num_chars_in);
	for (wptr = 2; wptr < 10; wptr++) {
		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(DAR_UTIL_INVALID_WPTR, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_in, bytes_out.num_chars);
		assert_memory_equal(expected, bytes_out.pkt_data, num_chars_in);
	}

	(void)state; // unused
}

static void DAR_add_rw_addr_addr_size_66_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_66;
	const uint32_t num_chars_in = 6;
	const uint32_t num_chars_out = 6 + 8;

	uint32_t addr[3] = {0xdeadbeef, 0xcafebabe, 0xa5a55a5a};
	uint32_t wptr;
	DAR_pkt_bytes_t bytes_out;

	uint32_t rc;
	uint8_t expected[RIO_MAX_PKT_BYTES];

	// valid wptr
	for (wptr = 0; wptr < 2; wptr++) {
		tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
				num_chars_in);

		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(0, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_out, bytes_out.num_chars);
		if (0 == wptr) {
			expected[num_chars_out - 1] = 0xea;
		} else {
			expected[num_chars_out - 1] = 0xee;
		}
		assert_memory_equal(expected, bytes_out.pkt_data,
				num_chars_out);
	}

	// invalid wptr
	tst_init_rw_addr_data(expected, &bytes_out, pkt_addr_size,
			num_chars_in);
	for (wptr = 2; wptr < 10; wptr++) {
		rc = DAR_add_rw_addr(pkt_addr_size, addr, wptr, &bytes_out);
		assert_int_equal(DAR_UTIL_INVALID_WPTR, rc);
		assert_int_equal(pkt_addr_size, bytes_out.pkt_addr_size);
		assert_true(bytes_out.pkt_has_crc);
		assert_false(bytes_out.pkt_padded);
		assert_int_equal(num_chars_in, bytes_out.num_chars);
		assert_memory_equal(expected, bytes_out.pkt_data, num_chars_in);
	}

	(void)state; // unused
}

static void tst_init_get_rw_addr_data(DAR_pkt_fields_t *fields_out,
		DAR_pkt_bytes_t *bytes_in, rio_addr_size pkt_addr_size,
		uint32_t *bidx)
{
	uint32_t idx = 0;

	memset(fields_out, 0, sizeof(DAR_pkt_fields_t));
	fields_out->tot_bytes = 0x123;
	fields_out->pad_bytes = 0x321;
	fields_out->log_rw.pkt_addr_size = rio_addr_66;
	fields_out->log_rw.addr[0] = 0xdeadbeef;
	fields_out->log_rw.addr[1] = 0xcafebabe;
	fields_out->log_rw.addr[2] = 0xa5a55a5a;

	memset(bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in->pkt_addr_size = pkt_addr_size;
	bytes_in->pkt_data[idx++] = 0xde;
	bytes_in->pkt_data[idx++] = 0xad;
	bytes_in->pkt_data[idx++] = 0xbe;
	bytes_in->pkt_data[idx++] = 0xef;
	bytes_in->pkt_data[idx++] = 0xca;
	bytes_in->pkt_data[idx++] = 0xfe;
	bytes_in->pkt_data[idx++] = 0xba;
	bytes_in->pkt_data[idx++] = 0xbe;
	bytes_in->pkt_data[idx++] = 0xa5;
	bytes_in->pkt_data[idx++] = 0x5a;
	bytes_in->pkt_data[idx++] = 0xe7;
	*bidx = idx;
}

static void DAR_get_rw_addr_addr_size_21_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_21;
	const uint32_t consumed = 3;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t bidx;
	uint32_t rc;

	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 9;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xbeefc8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	// again
	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 8;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xefcaf8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	(void)state; // unused
}

static void DAR_get_rw_addr_addr_size_32_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_32;
	const uint32_t consumed = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t bidx;
	uint32_t rc;

	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 9;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xbeefcaf8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	// again
	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 8;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xefcafeb8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	(void)state; // unused
}

static void DAR_get_rw_addr_addr_size_34_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_34;
	const uint32_t consumed = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t bidx;
	uint32_t rc;

	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 9;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xbeefcaf8, fields_out.log_rw.addr[0]);
	assert_int_equal(2, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	// again
	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 8;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xefcafeb8, fields_out.log_rw.addr[0]);
	assert_int_equal(2, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	(void)state; // unused
}

static void DAR_get_rw_addr_addr_size_50_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_50;
	const uint32_t consumed = 6;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t bidx;
	uint32_t rc;

	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 9;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xcafebab8, fields_out.log_rw.addr[0]);
	assert_int_equal(0x2beef, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	// again
	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 8;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xfebabea0, fields_out.log_rw.addr[0]);
	assert_int_equal(0x1efca, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	(void)state; // unused
}

static void DAR_get_rw_addr_addr_size_66_test(void **state)
{
	const rio_addr_size pkt_addr_size = rio_addr_66;
	const uint32_t consumed = 8;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t bidx;
	uint32_t rc;

	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 9;
	fields_out.log_rw.pkt_addr_size = rio_addr_21;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xbabea558, fields_out.log_rw.addr[0]);
	assert_int_equal(0xbeefcafe, fields_out.log_rw.addr[1]);
	assert_int_equal(2, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	// again
	tst_init_get_rw_addr_data(&fields_out, &bytes_in, pkt_addr_size, &bidx);
	bidx -= 8;
	fields_out.log_rw.pkt_addr_size = rio_addr_21;

	assert_int_equal(0xdeadbeef, fields_out.log_rw.addr[0]);
	assert_int_equal(0xcafebabe, fields_out.log_rw.addr[1]);
	assert_int_equal(0xa5a55a5a, fields_out.log_rw.addr[2]);

	rc = DAR_get_rw_addr(&bytes_in, &fields_out, bidx);
	assert_int_equal(bidx + consumed, rc);
	assert_int_equal(0xbea55ae0, fields_out.log_rw.addr[0]);
	assert_int_equal(0xefcafeba, fields_out.log_rw.addr[1]);
	assert_int_equal(3, fields_out.log_rw.addr[2]);
	assert_int_equal(0x123, fields_out.tot_bytes);
	assert_int_equal(0x321, fields_out.pad_bytes);
	assert_int_equal(pkt_addr_size, fields_out.log_rw.pkt_addr_size);

	(void)state; // unused
}

static void DAR_addr_size_addr_size_21_roundtrip_test(void **state)
{
	// assuming that the DAR_get_rw_addr_addr_size_XX_test and
	// DAR_add_rw_addr_addr_size_XX_test all pass, then a round
	// trip should also logically pass
	const uint8_t input[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe};
	const rio_addr_size pkt_addr_size = rio_addr_21;
	const uint32_t ret_idx = 3;

	DAR_pkt_bytes_t bytes_out;
	DAR_pkt_fields_t fields_out;
	DAR_pkt_bytes_t bytes_in;
	uint32_t idx;
	uint32_t lsb;
	uint32_t rc;

	// wptr = 0
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	assert_int_equal(0xdeadb8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);

	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, bytes_in.pkt_data[0]);
	assert_int_equal(0xad, bytes_in.pkt_data[1]);
	assert_int_equal(0xbe, bytes_in.pkt_data[2]);
	bytes_in.pkt_data[2] = 0xb8;
	assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data, ret_idx);

	// wptr = 1
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	assert_int_equal(0xdeadb8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);

	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, bytes_in.pkt_data[0]);
	assert_int_equal(0xad, bytes_in.pkt_data[1]);
	assert_int_equal(0xbe, bytes_in.pkt_data[2]);
	bytes_in.pkt_data[2] = 0xbc;
	assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data, ret_idx);

	// alignment
	for (lsb = 0; lsb < 0x10; lsb++) {
		// wptr = 0
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[2] = 0xb0 + lsb;

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		if (lsb < 8) {
			assert_int_equal(0xdeadb0, fields_out.log_rw.addr[0]);
		} else {
			assert_int_equal(0xdeadb8, fields_out.log_rw.addr[0]);
		}
		assert_int_equal(0, fields_out.log_rw.addr[1]);
		assert_int_equal(0, fields_out.log_rw.addr[2]);

		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_int_equal(0xde, bytes_in.pkt_data[0]);
		assert_int_equal(0xad, bytes_in.pkt_data[1]);
		assert_int_equal(0xb0 + lsb, bytes_in.pkt_data[2]);
		if (lsb < 8) {
			bytes_in.pkt_data[2] = 0xb0;
		} else {
			bytes_in.pkt_data[2] = 0xb8;
		}
		assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data,
				ret_idx);

		// wptr = 1
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[2] = 0xb0 + lsb;

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		if (lsb < 8) {
			assert_int_equal(0xdeadb0, fields_out.log_rw.addr[0]);
		} else {
			assert_int_equal(0xdeadb8, fields_out.log_rw.addr[0]);
		}
		assert_int_equal(0, fields_out.log_rw.addr[1]);
		assert_int_equal(0, fields_out.log_rw.addr[2]);

		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_int_equal(0xde, bytes_in.pkt_data[0]);
		assert_int_equal(0xad, bytes_in.pkt_data[1]);
		assert_int_equal(0xb0 + lsb, bytes_in.pkt_data[2]);
		if (lsb < 8) {
			bytes_in.pkt_data[2] = 0xb4;
		} else {
			bytes_in.pkt_data[2] = 0xbc;
		}
		assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data,
				ret_idx);
	}

	(void)state; // unused
}

static void DAR_addr_size_addr_size_32_roundtrip_test(void **state)
{
	// assuming that the DAR_get_rw_addr_addr_size_XX_test and
	// DAR_add_rw_addr_addr_size_XX_test all pass, then a round
	// trip should also logically pass
	const uint8_t input[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe};
	const rio_addr_size pkt_addr_size = rio_addr_32;
	const uint32_t ret_idx = 4;

	DAR_pkt_bytes_t bytes_out;
	DAR_pkt_fields_t fields_out;
	DAR_pkt_bytes_t bytes_in;
	uint32_t idx;
	uint32_t lsb;
	uint32_t rc;

	// wptr = 0
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	assert_int_equal(0xdeadbee8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);

	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	bytes_in.pkt_data[3] = 0xe8;
	assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data, ret_idx);

	// wptr = 1
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	assert_int_equal(0xdeadbee8, fields_out.log_rw.addr[0]);
	assert_int_equal(0, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);

	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	bytes_in.pkt_data[3] = 0xec;
	assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data, ret_idx);

	// alignment
	for (lsb = 0; lsb < 0x10; lsb++) {
		// wptr = 0
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[0] = 0xd0 + lsb;

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		assert_int_equal(((0xd0 | lsb) << 24) | 0xadbee8,
				fields_out.log_rw.addr[0]);
		assert_int_equal(0, fields_out.log_rw.addr[1]);
		assert_int_equal(0, fields_out.log_rw.addr[2]);

		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_int_equal(0xd0 | lsb, bytes_in.pkt_data[0]);
		bytes_in.pkt_data[3] = 0xe8;
		assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data,
				ret_idx);

		// wptr = 1
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[0] = 0xd0 + lsb;

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		assert_int_equal(((0xd0 | lsb) << 24) | 0xadbee8,
				fields_out.log_rw.addr[0]);
		assert_int_equal(0, fields_out.log_rw.addr[1]);
		assert_int_equal(0, fields_out.log_rw.addr[2]);

		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_int_equal(0xd0 | lsb, bytes_in.pkt_data[0]);
		bytes_in.pkt_data[3] = 0xec;
		assert_memory_equal(&bytes_in.pkt_data, &bytes_out.pkt_data,
				ret_idx);
	}

	(void)state; // unused
}

static void DAR_addr_size_addr_size_34_roundtrip_test(void **state)
{
	// assuming that the DAR_get_rw_addr_addr_size_XX_test and
	// DAR_add_rw_addr_addr_size_XX_test all pass, then a round
	// trip should also logically pass
	const uint8_t input[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe};
	const rio_addr_size pkt_addr_size = rio_addr_34;
	const uint32_t ret_idx = 4;

	DAR_pkt_bytes_t bytes_out;
	DAR_pkt_fields_t fields_out;
	DAR_pkt_bytes_t bytes_in;
	uint8_t output[sizeof(input)];
	uint32_t idx;
	uint32_t lsb;
	uint32_t rc;

	// wptr = 0
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	memcpy(&output, &input, sizeof(output));
	bytes_in.pkt_addr_size = pkt_addr_size;
	output[ret_idx - 1] = 0xeb;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	assert_int_equal(0xdeadbee8, fields_out.log_rw.addr[0]);
	assert_int_equal(3, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);

	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&input, &bytes_in.pkt_data, sizeof(input));
	assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

	// wptr = 1
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	memcpy(&output, &input, sizeof(output));
	bytes_in.pkt_addr_size = pkt_addr_size;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	assert_int_equal(0xdeadbee8, fields_out.log_rw.addr[0]);
	assert_int_equal(3, fields_out.log_rw.addr[1]);
	assert_int_equal(0, fields_out.log_rw.addr[2]);

	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&input, &bytes_in.pkt_data, sizeof(input));
	assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

	// alignment
	for (lsb = 0; lsb < 0x10; lsb++) {
		// wptr = 0
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[ret_idx - 1] = 0xd0 + lsb;

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		if (lsb < 8) {
			assert_int_equal(0xdeadbed0, fields_out.log_rw.addr[0]);
		} else {
			assert_int_equal(0xdeadbed8, fields_out.log_rw.addr[0]);
		}
		assert_int_equal(lsb % 4, fields_out.log_rw.addr[1]);
		assert_int_equal(0, fields_out.log_rw.addr[2]);

		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		memcpy(&output, &input, sizeof(output));
		output[ret_idx - 1] = 0xd0 | lsb;
		assert_memory_equal(&output, &bytes_in.pkt_data, sizeof(input));
		if (lsb < 8) {
			output[ret_idx - 1] = 0xd0 | (lsb % 4);
		} else {
			output[ret_idx - 1] = 0xd0 | ((lsb % 4) + 8);
		}
		assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

		// wptr = 1
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[ret_idx - 1] = 0xd0 + lsb;

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		if (lsb < 8) {
			assert_int_equal(0xdeadbed0, fields_out.log_rw.addr[0]);
		} else {
			assert_int_equal(0xdeadbed8, fields_out.log_rw.addr[0]);
		}
		assert_int_equal(lsb % 4, fields_out.log_rw.addr[1]);
		assert_int_equal(0, fields_out.log_rw.addr[2]);

		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		memcpy(&output, &input, sizeof(output));
		output[ret_idx - 1] = 0xd0 | lsb;
		assert_memory_equal(&output, &bytes_in.pkt_data, sizeof(input));
		if (lsb < 8) {
			output[ret_idx - 1] = 0xd0 | ((lsb % 4) + 4);
		} else {
			output[ret_idx - 1] = 0xd0 | ((lsb % 4) + 12);
		}
		assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);
	}

	(void)state; // unused
}

static void DAR_addr_size_addr_size_50_roundtrip_test(void **state)
{
	// assuming that the DAR_get_rw_addr_addr_size_XX_test and
	// DAR_add_rw_addr_addr_size_XX_test all pass, then a round
	// trip should also logically pass
	const uint8_t input[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe};
	const rio_addr_size pkt_addr_size = rio_addr_50;
	const uint32_t ret_idx = 6;

	DAR_pkt_bytes_t bytes_out;
	DAR_pkt_fields_t fields_out;
	DAR_pkt_bytes_t bytes_in;
	uint8_t output[sizeof(input)];
	uint32_t idx;
	uint32_t lsb;
	uint32_t rc;

	// wptr = 0
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	memcpy(&output, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;
	output[ret_idx - 1] = 0xfa;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

	// wptr = 1
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	memcpy(&output, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

	// alignment
	for (lsb = 0; lsb < 0x10; lsb++) {
		// wptr = 0
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		memcpy(&output, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[ret_idx - 1] = 0xf0 + lsb;
		if (lsb < 8) {
			output[ret_idx - 1] = 0xf0 | (lsb % 4);
		} else {
			output[ret_idx - 1] = 0xf0 | ((lsb % 4) + 8);
		}

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

		// wptr = 1
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		memcpy(&output, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[ret_idx - 1] = 0xf0 + lsb;
		if (lsb < 8) {
			output[ret_idx - 1] = 0xf0 | ((lsb % 4) + 4);
		} else {
			output[ret_idx - 1] = 0xf0 | ((lsb % 4) + 12);
		}

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);
	}

	(void)state; // unused
}

static void DAR_addr_size_addr_size_66_roundtrip_test(void **state)
{
	// assuming that the DAR_get_rw_addr_addr_size_XX_test and
	// DAR_add_rw_addr_addr_size_XX_test all pass, then a round
	// trip should also logically pass
	const uint8_t input[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe};
	const rio_addr_size pkt_addr_size = rio_addr_66;
	const uint32_t ret_idx = 8;

	DAR_pkt_bytes_t bytes_out;
	DAR_pkt_fields_t fields_out;
	DAR_pkt_bytes_t bytes_in;
	uint8_t output[sizeof(input)];
	uint32_t idx;
	uint32_t lsb;
	uint32_t rc;

	// wptr = 0
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	memcpy(&output, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;
	output[ret_idx - 1] = 0xba;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

	// wptr = 1
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	memcpy(&output, &input, sizeof(input));
	bytes_in.pkt_addr_size = pkt_addr_size;

	idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
	assert_int_equal(ret_idx, idx);
	rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
			&bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

	// alignment
	for (lsb = 0; lsb < 0x10; lsb++) {
		// wptr = 0
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		memcpy(&output, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[ret_idx - 1] = 0xb0 + lsb;
		if (lsb < 8) {
			output[ret_idx - 1] = 0xb0 | (lsb % 4);
		} else {
			output[ret_idx - 1] = 0xb0 | ((lsb % 4) + 8);
		}

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 0,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);

		// wptr = 1
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));
		memset(&bytes_in, 0, sizeof(DAR_pkt_fields_t));
		memcpy(&bytes_in.pkt_data, &input, sizeof(input));
		memcpy(&output, &input, sizeof(input));
		bytes_in.pkt_addr_size = pkt_addr_size;
		bytes_in.pkt_data[ret_idx - 1] = 0xb0 + lsb;
		if (lsb < 8) {
			output[ret_idx - 1] = 0xb0 | ((lsb % 4) + 4);
		} else {
			output[ret_idx - 1] = 0xb0 | ((lsb % 4) + 12);
		}

		idx = DAR_get_rw_addr(&bytes_in, &fields_out, 0);
		assert_int_equal(ret_idx, idx);
		rc = DAR_add_rw_addr(pkt_addr_size, fields_out.log_rw.addr, 1,
				&bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_memory_equal(&output, &bytes_out.pkt_data, ret_idx);
	}

	(void)state; // unused
}

static void count_bits_test(void **state)
{
	assert_int_equal(0, count_bits(0x0));
	assert_int_equal(1, count_bits(0x1));
	assert_int_equal(1, count_bits(0x2));
	assert_int_equal(2, count_bits(0x3));
	assert_int_equal(1, count_bits(0x4));
	assert_int_equal(2, count_bits(0x5));
	assert_int_equal(2, count_bits(0x6));
	assert_int_equal(3, count_bits(0x7));
	assert_int_equal(1, count_bits(0x8));
	assert_int_equal(2, count_bits(0x9));
	assert_int_equal(2, count_bits(0xa));
	assert_int_equal(3, count_bits(0xb));
	assert_int_equal(2, count_bits(0xc));
	assert_int_equal(3, count_bits(0xd));
	assert_int_equal(3, count_bits(0xe));
	assert_int_equal(4, count_bits(0xf));

	assert_int_equal(4, count_bits(0xf0));
	assert_int_equal(5, count_bits(0xf1));
	assert_int_equal(5, count_bits(0xf2));
	assert_int_equal(6, count_bits(0xf3));
	assert_int_equal(5, count_bits(0xf4));
	assert_int_equal(6, count_bits(0xf5));
	assert_int_equal(6, count_bits(0xf6));
	assert_int_equal(7, count_bits(0xf7));
	assert_int_equal(5, count_bits(0xf8));
	assert_int_equal(6, count_bits(0xf9));
	assert_int_equal(6, count_bits(0xfa));
	assert_int_equal(7, count_bits(0xfb));
	assert_int_equal(6, count_bits(0xfc));
	assert_int_equal(7, count_bits(0xfd));
	assert_int_equal(7, count_bits(0xfe));
	assert_int_equal(8, count_bits(0xff));

	// Only 16 bits count
	assert_int_equal(13, count_bits(0xbeef));
	assert_int_equal(13, count_bits(0xfeeb));
	assert_int_equal(13, count_bits(0xebef));
	assert_int_equal(13, count_bits(0xdeadbeef));

	assert_int_equal(11, count_bits(0xcafe));
	assert_int_equal(11, count_bits(0xacfe));
	assert_int_equal(11, count_bits(0xefca));

	(void)state; // unused
}

static void CS_fields_to_bytes_small_test(void **state)
{
	const uint32_t size = 8;
	const rio_cs_size cs_size = cs_small;
	const uint8_t pattern_a5[size] = {0x1c, 0xa5, 0xa5, 0xab};
	const uint8_t pattern_5a[size] = {0x7c, 0x5a, 0x5a, 0x52};

	CS_field_t fields_in;
	CS_bytes_t bytes_out;
	uint8_t expected[size];
	uint32_t rc;
	uint32_t idx;

	// invalid size
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	memset(expected, 0, size);
	fields_in.cs_size = cs_invalid;
	rc = CS_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_ERR_INVALID_PARAMETER, rc);
	assert_memory_equal(&expected, &bytes_out, size);

	// generate a5a5 pattern with valid crc
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	fields_in.cs_size = cs_size;
	fields_in.cs_t0 = stype0_VC_status;
	fields_in.parm_0 = 0x5;
	fields_in.parm_1 = 0x14;
	fields_in.cs_t1 = stype1_mecs;
	fields_in.cs_t1_cmd = 0x5;
	fields_in.cs_t2 = stype2_rsvd2; // ignored

	rc = CS_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, bytes_out.cs_type_valid);
	assert_memory_equal(&pattern_a5, bytes_out.cs_bytes, 8);

	// generate 5a5a pattern with valid crc
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	fields_in.cs_size = cs_size;
	fields_in.cs_t0 = stype0_pna;
	fields_in.parm_0 = 0x1a;
	fields_in.parm_1 = 0x0b;
	fields_in.cs_t1 = stype1_eop;
	fields_in.cs_t1_cmd = 0x2;
	fields_in.cs_t2 = stype2_rsvd2; // ignored

	rc = CS_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, bytes_out.cs_type_valid);
	assert_memory_equal(&pattern_5a, bytes_out.cs_bytes, 8);

	// ensure first byte is correct (not going to look at rest)
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	fields_in.cs_size = cs_size;
	fields_in.cs_t0 = stype0_pna;
	fields_in.parm_0 = 0x1a;
	fields_in.parm_1 = 0x0b;
	fields_in.cs_t1_cmd = 0x2;
	for (idx = stype1_sop; idx <= stype1_nop; idx++) {
		bytes_out.cs_bytes[0] = 0;
		fields_in.cs_t1 = (stype1)idx;

		rc = CS_fields_to_bytes(&fields_in, &bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		switch (idx) {
		case 0:
		case 1:
		case 2:
		case 4:
			assert_int_equal(PD_CONTROL_SYMBOL,
					bytes_out.cs_bytes[0]);
			break;
		case 3:
		case 5:
		case 6:
		case 7:
			assert_int_equal(SC_START_CONTROL_SYMBOL,
					bytes_out.cs_bytes[0]);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void CS_fields_to_bytes_large_test(void **state)
{
	const uint32_t size = 8;
	const rio_cs_size cs_size = cs_large;
	const uint8_t pattern_a5[size] = {0x1c, 0xa5, 0xa5, 0xa5, 0xa5, 0xac,
			0x4c, 0x1c};
	const uint8_t pattern_5a[size] = {0x7c, 0x5a, 0x5a, 0x5a, 0x5a, 0x5c,
			0xbc, 0x7c};

	CS_field_t fields_in;
	CS_bytes_t bytes_out;
	uint8_t expected[size];
	uint32_t rc;
	uint32_t idx;

	// invalid size
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	memset(expected, 0, size);
	fields_in.cs_size = cs_invalid;
	rc = CS_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_ERR_INVALID_PARAMETER, rc);
	assert_memory_equal(&expected, &bytes_out, size);

	// generate a5a5 pattern with crc
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	fields_in.cs_size = cs_size;
	fields_in.cs_t0 = stype0_VC_status;
	fields_in.parm_0 = 0x0b;
	fields_in.parm_1 = 0x12;
	fields_in.cs_t1 = stype1_rsvd;
	fields_in.cs_t1_cmd = 0x4;
	fields_in.cs_t2 = stype2_rsvd5;
	fields_in.cs_t2_val = 0x52d;

	rc = CS_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, bytes_out.cs_type_valid);
	assert_memory_equal(&pattern_a5, bytes_out.cs_bytes, 8);

	// generate 5a5a pattern with valid crc
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	fields_in.cs_size = cs_size;
	fields_in.cs_t0 = stype0_pna;
	fields_in.parm_0 = 0x34;
	fields_in.parm_1 = 0x2d;
	fields_in.cs_t1 = stype1_stomp;
	fields_in.cs_t1_cmd = 3;
	fields_in.cs_t2 = stype2_rsvd2;
	fields_in.cs_t2_val = 0x2d2;

	rc = CS_fields_to_bytes(&fields_in, &bytes_out);

	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, bytes_out.cs_type_valid);
	assert_memory_equal(&pattern_5a, bytes_out.cs_bytes, 8);

	// ensure first byte is correct (not going to look at rest)
	memset(&bytes_out, 0, sizeof(CS_bytes_t));
	fields_in.cs_size = cs_size;
	fields_in.cs_t0 = stype0_pna;
	fields_in.parm_0 = 0x1a;
	fields_in.parm_1 = 0x0b;
	fields_in.cs_t1_cmd = 0x2;
	for (idx = stype1_sop; idx <= stype1_nop; idx++) {
		bytes_out.cs_bytes[0] = 0;
		fields_in.cs_t1 = (stype1)idx;

		rc = CS_fields_to_bytes(&fields_in, &bytes_out);
		assert_int_equal(RIO_SUCCESS, rc);
		switch (idx) {
		case 0:
		case 1:
		case 2:
		case 4:
			assert_int_equal(PD_CONTROL_SYMBOL,
					bytes_out.cs_bytes[0]);
			break;
		case 3:
		case 5:
		case 6:
		case 7:
			assert_int_equal(SC_START_CONTROL_SYMBOL,
					bytes_out.cs_bytes[0]);
			break;
		default:
			fail_msg("Invalid index %u", idx);
		}
	}

	(void)state; // unused
}

static void CS_bytes_to_fields_small_test(void **state)
{
	const uint8_t size = 4;
	const rio_cs_size cs_size = cs_small;
	const uint8_t pattern_a5[size] = {0x1c, 0xa5, 0xa5, 0xab};
	const uint8_t pattern_5a[size] = {0x7c, 0x5a, 0x5a, 0x52};

	CS_bytes_t bytes_in;
	CS_field_t fields_out;
	uint32_t rc;

	// generate a5a5 pattern with invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0xa5, sizeof(bytes_in.cs_bytes));

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0x5, fields_out.parm_0);
	assert_int_equal(0x14, fields_out.parm_1);
	assert_int_equal(stype1_mecs, fields_out.cs_t1);
	assert_int_equal(0x5, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate a5a5 pattern with crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0xa5, sizeof(bytes_in.cs_bytes));
	bytes_in.cs_bytes[2] = 0xab;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0x5, fields_out.parm_0);
	assert_int_equal(0x14, fields_out.parm_1);
	assert_int_equal(stype1_mecs, fields_out.cs_t1);
	assert_int_equal(0x5, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);

	// generate a5a5 pattern with control symbol and invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_a5, size);
	bytes_in.cs_bytes[size - 1] = 0xa5;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0x5, fields_out.parm_0);
	assert_int_equal(0x14, fields_out.parm_1);
	assert_int_equal(stype1_mecs, fields_out.cs_t1);
	assert_int_equal(0x5, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate a5a5 pattern with control symbol and crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_a5, size);

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0x5, fields_out.parm_0);
	assert_int_equal(0x14, fields_out.parm_1);
	assert_int_equal(stype1_mecs, fields_out.cs_t1);
	assert_int_equal(0x5, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);

	// generate 5a5a pattern with invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0x5a, sizeof(bytes_in.cs_bytes));

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x1a, fields_out.parm_0);
	assert_int_equal(0x0b, fields_out.parm_1);
	assert_int_equal(stype1_eop, fields_out.cs_t1);
	assert_int_equal(0x2, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate 5a5a pattern with crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0x5a, sizeof(bytes_in.cs_bytes));
	bytes_in.cs_bytes[2] = 0x52;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x1a, fields_out.parm_0);
	assert_int_equal(0x0b, fields_out.parm_1);
	assert_int_equal(stype1_eop, fields_out.cs_t1);
	assert_int_equal(0x2, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);

	// generate 5a5a pattern with control symbol and invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_5a, size);
	bytes_in.cs_bytes[size - 1] = 0x5a;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x1a, fields_out.parm_0);
	assert_int_equal(0x0b, fields_out.parm_1);
	assert_int_equal(stype1_eop, fields_out.cs_t1);
	assert_int_equal(0x2, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate 5a5a pattern with control symbol and crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_5a, size);

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x1a, fields_out.parm_0);
	assert_int_equal(0x0b, fields_out.parm_1);
	assert_int_equal(stype1_eop, fields_out.cs_t1);
	assert_int_equal(0x2, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_nop, fields_out.cs_t2);
	assert_int_equal(0, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);

	(void)state; // unused
}

static void CS_bytes_to_fields_large_test(void **state)
{

	const uint8_t size = 8;
	const rio_cs_size cs_size = cs_large;
	const uint8_t pattern_a5[size] = {0x1c, 0xa5, 0xa5, 0xa5, 0xa5, 0xac,
			0x4c, 0x1c};
	const uint8_t pattern_5a[size] = {0x7c, 0x5a, 0x5a, 0x5a, 0x5a, 0x5c,
			0xbc, 0x7c};

	CS_bytes_t bytes_in;
	CS_field_t fields_out;
	uint32_t rc;

	// generate a5a5 pattern with invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0xa5, sizeof(bytes_in.cs_bytes));

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0xb, fields_out.parm_0);
	assert_int_equal(0x12, fields_out.parm_1);
	assert_int_equal(stype1_rsvd, fields_out.cs_t1);
	assert_int_equal(0x4, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd5, fields_out.cs_t2);
	assert_int_equal(0x52d, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate a5a5 pattern with crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0xa5, sizeof(bytes_in.cs_bytes));
	bytes_in.cs_bytes[4] = 0xac;
	bytes_in.cs_bytes[5] = 0x4c;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0xb, fields_out.parm_0);
	assert_int_equal(0x12, fields_out.parm_1);
	assert_int_equal(stype1_rsvd, fields_out.cs_t1);
	assert_int_equal(0x4, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd5, fields_out.cs_t2);
	assert_int_equal(0x52d, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);

	// generate a5a5 pattern with control symbol and invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_a5, size);
	bytes_in.cs_bytes[size - 2] = 0xa5;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0xb, fields_out.parm_0);
	assert_int_equal(0x12, fields_out.parm_1);
	assert_int_equal(stype1_rsvd, fields_out.cs_t1);
	assert_int_equal(0x4, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd5, fields_out.cs_t2);
	assert_int_equal(0x52d, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate a5a5 pattern with control symbol and crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_a5, size);

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_VC_status, fields_out.cs_t0);
	assert_int_equal(0xb, fields_out.parm_0);
	assert_int_equal(0x12, fields_out.parm_1);
	assert_int_equal(stype1_rsvd, fields_out.cs_t1);
	assert_int_equal(0x4, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd5, fields_out.cs_t2);
	assert_int_equal(0x52d, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);;

	// generate 5a5a pattern with invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0x5a, sizeof(bytes_in.cs_bytes));

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x34, fields_out.parm_0);
	assert_int_equal(0x2d, fields_out.parm_1);
	assert_int_equal(stype1_stomp, fields_out.cs_t1);
	assert_int_equal(0x3, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd2, fields_out.cs_t2);
	assert_int_equal(0x2d2, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate 5a5a pattern with crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0x5a, sizeof(bytes_in.cs_bytes));
	bytes_in.cs_bytes[4] = 0x5c;
	bytes_in.cs_bytes[5] = 0xbc;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x34, fields_out.parm_0);
	assert_int_equal(0x2d, fields_out.parm_1);
	assert_int_equal(stype1_stomp, fields_out.cs_t1);
	assert_int_equal(0x3, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd2, fields_out.cs_t2);
	assert_int_equal(0x2d2, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);

	// generate 5a5a pattern with control symbol and invalid crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_5a, size);
	bytes_in.cs_bytes[size - 2] = 0x5a;

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x34, fields_out.parm_0);
	assert_int_equal(0x2d, fields_out.parm_1);
	assert_int_equal(stype1_stomp, fields_out.cs_t1);
	assert_int_equal(0x3, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd2, fields_out.cs_t2);
	assert_int_equal(0x2d2, fields_out.cs_t2_val);
	assert_false(fields_out.cs_crc_correct);

	// generate 5a5a pattern with control symbol and crc
	bytes_in.cs_type_valid = cs_size;
	memset(&bytes_in.cs_bytes, 0, sizeof(bytes_in.cs_bytes));
	memcpy(&bytes_in.cs_bytes, pattern_5a, size);

	rc = CS_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(cs_size, fields_out.cs_size);
	assert_int_equal(stype0_pna, fields_out.cs_t0);
	assert_int_equal(0x34, fields_out.parm_0);
	assert_int_equal(0x2d, fields_out.parm_1);
	assert_int_equal(stype1_stomp, fields_out.cs_t1);
	assert_int_equal(0x3, fields_out.cs_t1_cmd);
	assert_int_equal(stype2_rsvd2, fields_out.cs_t2);
	assert_int_equal(0x2d2, fields_out.cs_t2_val);
	assert_true(fields_out.cs_crc_correct);

	(void)state; // unused
}

static void get_stype0_descr_test(void **state)
{
	// highlights if a string changes value
	CS_field_t fields_in;

	fields_in.cs_t0 = stype0_pa;
	assert_string_equal("Packet-Accepted", get_stype0_descr(&fields_in));

	fields_in.cs_t0 = stype0_rty;
	assert_string_equal("Packet-Retry", get_stype0_descr(&fields_in));

	fields_in.cs_t0 = stype0_pna;
	assert_string_equal("Packet-no-Accepted", get_stype0_descr(&fields_in));

	fields_in.cs_t0 = stype0_rsvd;
	assert_string_equal("Reserved", get_stype0_descr(&fields_in));

	fields_in.cs_t0 = stype0_status;
	assert_string_equal("Status", get_stype0_descr(&fields_in));

	fields_in.cs_t0 = stype0_VC_status;
	assert_string_equal("VC_Status", get_stype0_descr(&fields_in));

	fields_in.cs_t0 = stype0_lresp;
	assert_string_equal("Link_response", get_stype0_descr(&fields_in));

	fields_in.cs_t0 = stype0_imp;
	assert_string_equal("Imp_Spec", get_stype0_descr(&fields_in));

	(void)state; // unused
}

static void get_stype0_PNA_cause_parm1_test(void **state)
{
	// highlights if a string changes value
	CS_field_t fields_in;
	uint32_t idx;

	for (idx = stype0_min; idx <= stype0_max; idx++) {
		if (idx == stype0_pna) {
			continue;
		}
		fields_in.cs_t0 = (stype0)idx;
		assert_null(get_stype0_PNA_cause_parm1(&fields_in));
	}

	fields_in.cs_t0 = stype0_pna;
	for (idx = 0; idx < 0xff; idx++) { // upb is arbitrary
		fields_in.parm_1 = idx;
		switch (idx) {
		case 0:
			assert_string_equal("Reserved",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 1:
			assert_string_equal("Unexpected AckID",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 2:
			assert_string_equal("Bad CS CRC",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 3:
			assert_string_equal("Non-mtc RX stopped",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 4:
			assert_string_equal("Bad Pkt CRC",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 5:
			assert_string_equal("Bad 10b char",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 6:
			assert_string_equal("Lack of resources",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 7:
			assert_string_equal("Lost Descrambler Sync",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		case 0x1f:
			assert_string_equal("General Error",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		default:
			assert_string_equal("Reserved",
					(get_stype0_PNA_cause_parm1(&fields_in)));
			break;
		}
	}

	(void)state; // unused
}

static void get_stype0_LR_port_status_parm1_test(void **state)
{
	// highlights if a string changes value
	CS_field_t fields_in;
	uint32_t idx;

	for (idx = stype0_min; idx <= stype0_max; idx++) {
		if (idx == stype0_lresp) {
			continue;
		}
		fields_in.cs_t0 = (stype0)idx;
		assert_null(get_stype0_LR_port_status_parm1(&fields_in));
	}

	fields_in.cs_t0 = stype0_lresp;
	for (idx = 0; idx < 0xff; idx++) { // upb is arbitrary
		fields_in.parm_1 = idx;
		switch (fields_in.parm_1) {
		case 2:
			assert_string_equal("Error",
					(get_stype0_LR_port_status_parm1(
							&fields_in)));
			break;
		case 4:
			assert_string_equal("Retry-stopped",
					(get_stype0_LR_port_status_parm1(
							&fields_in)));
			break;
		case 5:
			assert_string_equal("Error-stopped",
					(get_stype0_LR_port_status_parm1(
							&fields_in)));
			break;
		case 0x10:
			assert_string_equal("OK",
					(get_stype0_LR_port_status_parm1(
							&fields_in)));
			break;
		default:
			assert_string_equal("Reserved",
					(get_stype0_LR_port_status_parm1(
							&fields_in)));
			break;
		}
	}

	(void)state; // unused
}

static void get_stype1_descr_test(void **state)
{
	// highlights if a string changes value
	CS_field_t fields_in;

	fields_in.cs_t1 = stype1_sop;
	assert_string_equal("Start of Packet", get_stype1_descr(&fields_in));

	fields_in.cs_t1 = stype1_stomp;
	assert_string_equal("Stomp", get_stype1_descr(&fields_in));

	fields_in.cs_t1 = stype1_eop;
	assert_string_equal("End-of-packet", get_stype1_descr(&fields_in));

	fields_in.cs_t1 = stype1_rfr;
	assert_string_equal("restart-from-retry", get_stype1_descr(&fields_in));

	fields_in.cs_t1 = stype1_lreq;
	assert_string_equal("Link-Request", get_stype1_descr(&fields_in));

	fields_in.cs_t1 = stype1_mecs;
	assert_string_equal("Multicast-Event", get_stype1_descr(&fields_in));

	fields_in.cs_t1 = stype1_rsvd;
	assert_string_equal("Reserved", get_stype1_descr(&fields_in));

	fields_in.cs_t1 = stype1_nop;
	assert_string_equal("NOP", get_stype1_descr(&fields_in));

	(void)state; // unused
}

static void get_stype1_lreq_cmd_test(void **state)
{
	// highlights if a string changes value
	CS_field_t fields_in;
	uint32_t idx;

	for (idx = stype1_min; idx <= stype1_max; idx++) {
		if (idx == stype1_lreq) {
			continue;
		}
		fields_in.cs_t1 = (stype1)idx;
		assert_null(get_stype1_lreq_cmd(&fields_in));
	}

	fields_in.cs_t1 = stype1_lreq;
	for (idx = 0; idx < 0xff; idx++) {
		fields_in.cs_t1_cmd = idx;
		switch (fields_in.cs_t1_cmd) {
		case 3:
			assert_string_equal("reset-device",
					(get_stype1_lreq_cmd(&fields_in)));
			break;
		case 4:
			assert_string_equal("input-status",
					(get_stype1_lreq_cmd(&fields_in)));
			break;
		default:
			assert_string_equal("Reserved",
					(get_stype1_lreq_cmd(&fields_in)));
			break;
		}
	}

	(void)state; // unused
}

static void get_stype2_descr_test(void **state)
{
	// highlights if a string changes value
	CS_field_t fields_in;
	uint32_t idx;

	for (idx = stype2_min; idx <= stype2_max; idx++) {
		fields_in.cs_t2 = (stype2)idx;
		switch (idx) {
		case 0:
			assert_string_equal("NOP",
					(get_stype2_descr(&fields_in)));
			break;
		case 1:
			assert_string_equal("VoQ Backpressure",
					(get_stype2_descr(&fields_in)));
			break;
		default:
			assert_string_equal("Reserved",
					(get_stype2_descr(&fields_in)));
			break;
		}
	}

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_2_memsz_34_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x82, 0xde, 0xad, 0x44, 0xa1, 0xde,
			0xad, 0xbe, 0xeb, 0xa9, 0xc0};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0x4; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 2;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xde;
	fields_in.trans.srcID = 0xad;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_nr;
	fields_in.pkt_bytes = 2;
	fields_in.pkt_data = 0;

	fields_in.log_rw.pkt_addr_size = rio_addr_34;
	fields_in.log_rw.addr[0] = 0xdeadbee8;
	fields_in.log_rw.addr[1] = 0x3;
	fields_in.log_rw.addr[2] = 0x0;
	fields_in.log_rw.tid = 0xa1;
	fields_in.log_rw.status = pkt_err; // ignored

	// not used by packet
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(12, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_2_memsz_50_test(void **state)
{
	const uint8_t expected[] = {0x01, 0x52, 0xde, 0xad, 0xbe, 0xef, 0x44,
			0x1a, 0x00, 0x03, 0xca, 0xfe, 0xba, 0xbc, 0x84, 0xd1};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0x2; // skipped by algorithm
	fields_in.phys.pkt_vc = 0;
	fields_in.phys.crf = 1;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_large;
	fields_in.trans.destID = 0xdead;
	fields_in.trans.srcID = 0xbeef;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_nr;
	fields_in.pkt_bytes = 2;
	fields_in.pkt_data = 0;

	fields_in.log_rw.pkt_addr_size = rio_addr_50;
	fields_in.log_rw.addr[0] = 0xcafebabc;
	fields_in.log_rw.addr[1] = 0x3;
	fields_in.log_rw.addr[2] = 0x0;
	fields_in.log_rw.tid = 0x1a;
	fields_in.log_rw.status = pkt_err; // ignored

	// not used by packet
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(16, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_5_memsz_66_test(void **state)
{
	const uint8_t expected[] = {0x02, 0xd5, 0xca, 0xfe, 0xba, 0xbe, 0x54,
			0x67, 0xa5, 0x5a, 0xa5, 0x5a, 0xde, 0xad, 0xbe, 0xe8,
			0xa5, 0xa5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf0, 0x91};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0x1; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 3;

	fields_in.trans.tt_code = tt_large;
	fields_in.trans.destID = 0xcafe;
	fields_in.trans.srcID = 0xbabe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_nwr;
	fields_in.pkt_bytes = 2;

	fields_in.log_rw.pkt_addr_size = rio_addr_66;
	fields_in.log_rw.addr[0] = 0xdeadbee8;
	fields_in.log_rw.addr[1] = 0xa55aa55a;
	fields_in.log_rw.addr[2] = 0x0;
	fields_in.log_rw.tid = 0x67;
	fields_in.log_rw.status = pkt_err; // ignored

	// not used by packet
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	memset(&pkt_data, 0xa5, sizeof(pkt_data));
	fields_in.pkt_data = pkt_data;

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(28, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_true(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_6_memsz_32_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x46, 0xca, 0xfe, 0xde, 0xad, 0xbe,
			0xe8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x52, 0xc1};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_sw;
	fields_in.pkt_bytes = 2;

	fields_in.log_rw.pkt_addr_size = rio_addr_32;
	fields_in.log_rw.addr[0] = 0xdeadbee8;
	fields_in.log_rw.addr[1] = 0x0;
	fields_in.log_rw.addr[2] = 0x0;
	fields_in.log_rw.tid = 0x67; // not used
	fields_in.log_rw.status = pkt_err; // ignored

	// not used by packet
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(16, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_7_pkt_type_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x47, 0xca, 0xfe, 0x30, 0x3, 0xe5,
			0x46};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.log_fc.fc_destID = 0xde; // not used
	fields_in.log_fc.fc_srcID = 0xad; // not used
	fields_in.log_fc.fc_fam = fc_fam_011;
	fields_in.log_fc.fc_flow = fc_flow_0B;
	fields_in.log_fc.fc_soc_is_ep = true;
	fields_in.log_fc.fc_xon = false;

	fields_in.pkt_type = pkt_fc;
	fields_in.pkt_bytes = 2;

	// not used by packet
	memset(&fields_in.log_rw, 0xca, sizeof(fields_in.log_rw));
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(8, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_8_memsz_21_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x48, 0xca, 0xfe, 0x4, 0x49, 0xff, 0xad, 0xbe, 0xe8, 0x84, 0x57};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_mr;
	fields_in.pkt_bytes = 2;

	fields_in.log_rw.pkt_addr_size = rio_addr_21;
	fields_in.log_rw.addr[0] = 0xdeadbee8;
	fields_in.log_rw.addr[1] = 0x0;
	fields_in.log_rw.addr[2] = 0x0;
	fields_in.log_rw.tid = 0x49; // not used
	fields_in.log_rw.status = pkt_err; // ignored

	// not used by packet
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(12, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_9_pkt_type_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x49, 0xca, 0xfe, 0x0, 0xc, 0xca, 0xfe, 0x42, 0xbe, 0xef, 0x5a,
			  0xde, 0x6e};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_dstm;
	fields_in.pkt_bytes = 2;

	fields_in.log_ds.dstm_COS = 0;
	fields_in.log_ds.dstm_PDU_len = 0x03;
	fields_in.log_ds.dstm_end_seg = true;
	fields_in.log_ds.dstm_odd_data_amt = false;
	fields_in.log_ds.dstm_pad_data_amt = true;
	fields_in.log_ds.dstm_start_seg = false;
	fields_in.log_ds.dstm_streamid = 0xcafe;
	fields_in.log_ds.dstm_xh_COS_mask = 0xbabe;
	fields_in.log_ds.dstm_xh_parm1 = 0xdeadbeef;
	fields_in.log_ds.dstm_xh_parm2 = 0xa55aa55a;
	fields_in.log_ds.dstm_xh_seg = true;
	fields_in.log_ds.dstm_xh_tm_op = 0x1234;
	fields_in.log_ds.dstm_xh_type = 0x4321;
	fields_in.log_ds.dstm_xh_wildcard = 0x66aa55aa;

	// not used by packet
	memset(&fields_in.log_rw, 0xa5, sizeof(fields_in.log_rw));
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(16, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_true(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_10_pkt_type_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x4a, 0xca, 0xfe, 0x0, 0x1a, 0xa1, 0xa1, 0x57, 0xd0};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0xa1, 2);
	fields_in.pkt_data = pkt_data;
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_db;
	fields_in.pkt_bytes = 2;

	fields_in.log_rw.pkt_addr_size = rio_addr_32; // not used
	fields_in.log_rw.addr[0] = 0xdeadbee8; // not used
	fields_in.log_rw.addr[1] = 0xcaca; // not used
	fields_in.log_rw.addr[2] = 0xacac; // not used
	fields_in.log_rw.tid = 0x1a;
	fields_in.log_rw.status = pkt_err; // ignored

	// not used by packet
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(12, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_true(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_11_pkt_type_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x4b, 0xca, 0xfe, 0x19, 0x61, 0xa1, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x0, 0xd4, 0xac};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0xa1, 2);
	fields_in.pkt_data = pkt_data;
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_msg;
	fields_in.pkt_bytes = 8;

	fields_in.log_ms.letter = 1;
	fields_in.log_ms.mbid = 2;
	fields_in.log_ms.msg_len = 1;
	fields_in.log_ms.msgseg = 1;

	// not used by packet
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_rw, 0x5a, sizeof(fields_in.log_rw));
	fields_in.log_rw.addr[0] = 0xcafeb8;

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(16, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);

	(void)state; // unused
}
static void DAR_pkt_fields_to_bytes_ftype_13_pkt_type_test(void **state)
{
	const uint8_t expected[] = {0x02, 0x4d, 0xca, 0xfe, 0x10, 0x61, 0xe9, 0xea};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0xa1, 2);
	fields_in.pkt_data = pkt_data;
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_msg_resp;

	fields_in.log_ms.letter = 1;
	fields_in.log_ms.mbid = 2;
	fields_in.log_ms.msg_len = 1;
	fields_in.log_ms.msgseg = 1;

	// not used by packet
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_rw, 0x5a, sizeof(fields_in.log_rw));
	fields_in.log_rw.addr[0] = 0xcafeb8;

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(8, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_false(bytes_out.pkt_padded);

	(void)state; // unused
}

static void DAR_pkt_fields_to_bytes_ftype_raw_pkt_type_test(void **state)
{
	const uint8_t expected[] = {0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0x34, 0x3d};

	DAR_pkt_fields_t fields_in;
	DAR_pkt_bytes_t bytes_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	memset(&fields_in, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0xa1, 8);
	fields_in.pkt_data = pkt_data;
	memset(&bytes_out, 0, sizeof(DAR_pkt_bytes_t));

	fields_in.tot_bytes = 0xff;
	fields_in.pad_bytes = 0xff;

	fields_in.phys.pkt_ackID = 0xcf; // skipped by algorithm
	fields_in.phys.pkt_vc = 1;
	fields_in.phys.crf = 0;
	fields_in.phys.pkt_prio = 1;

	fields_in.trans.tt_code = tt_small;
	fields_in.trans.destID = 0xca;
	fields_in.trans.srcID = 0xfe;
	fields_in.trans.hopcount = 0xff; // ignored

	fields_in.pkt_type = pkt_raw;
	fields_in.pkt_bytes = 8;

	// not used by packet
	memset(&fields_in.log_ds, 0xa5, sizeof(fields_in.log_ds));
	memset(&fields_in.log_fc, 0xca, sizeof(fields_in.log_fc));
	memset(&fields_in.log_rw, 0x5a, sizeof(fields_in.log_rw));
	memset(&fields_in.log_ms, 0x5a, sizeof(fields_in.log_ms));
	fields_in.log_rw.addr[0] = 0xcafebee8;

	rc = DAR_pkt_fields_to_bytes(&fields_in, &bytes_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_memory_equal(&expected, &bytes_out.pkt_data, sizeof(expected));
	assert_int_equal(12, bytes_out.num_chars);
	assert_true(bytes_out.pkt_has_crc);
	assert_true(bytes_out.pkt_padded);

	(void)state; // unused
}

#define TST_MIN_NUM_CHARS 8
#define TST_INVLD_TT_CODE rio_TT_code_max

static void DAR_pkt_bytes_to_fields_invalid_tt_code_test(void **state)
{
	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	DAR_pkt_fields_t expected;

	uint32_t idx;
	uint32_t rc;

	// test isolates the tt_code and verifies all invalid values
	// ignores other fields and values
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&expected, 0, sizeof(DAR_pkt_fields_t));

	for (idx = rio_TT_code_min; idx <= rio_TT_code_max; idx++) {
		if ((tt_small == (rio_TT_code)idx)
				|| (tt_large == (rio_TT_code)idx)) {
			continue;
		}
		memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
		bytes_in.num_chars = TST_MIN_NUM_CHARS;
		bytes_in.pkt_data[1] = idx << 4;
		expected.trans.tt_code = (rio_TT_code)idx;

		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(DAR_UTIL_INVALID_TT, rc);
		assert_memory_equal(&expected, &fields_out,
				sizeof(DAR_pkt_fields_t));
	}

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_not_enough_bytes_test(void **state)
{
	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	DAR_pkt_fields_t expected;

	uint32_t idx;
	uint32_t rc;

	// test ensures the minimum number of bytes is provided
	// ensures in the event of failure the out parameter is not updated
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&expected, 0, sizeof(DAR_pkt_fields_t));

	for (idx = 0; idx < TST_MIN_NUM_CHARS; idx++) {
		memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
		bytes_in.num_chars = idx;
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(DAR_UTIL_BAD_DATA_SIZE, rc);
		assert_memory_equal(&expected, &fields_out,
				sizeof(DAR_pkt_fields_t));
	}

	// give enough bytes, but fail on the rio_TT_code for simplicity
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_data[1] = TST_INVLD_TT_CODE << 4;

	// not going to verify the fields_out data as that is the responsibility
	// of the test that ensure the rio_TT_code is handled correctly
	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(DAR_UTIL_INVALID_TT, rc);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_bytes_0_and_1_test(void **state)
{
	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;

	uint32_t rc;

	// verify the contents of bytes 0 and 1, fail on a rio_TT_code error for simplicity
	// ignores subsequent fields and values
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_data[0] = 0xa5;
	bytes_in.pkt_data[1] = 0xbd;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(DAR_UTIL_INVALID_TT, rc);
	assert_int_equal(0x29, fields_out.phys.pkt_ackID);
	assert_int_equal(0x0, fields_out.phys.pkt_vc);
	assert_int_equal(0x1, fields_out.phys.crf);
	assert_int_equal(0x2, fields_out.phys.pkt_prio);
	assert_int_equal(0x3, fields_out.trans.tt_code);

	// verify the contents of bytes 0 and 1, fail on a rio_TT_code error for simplicity
	// ignores subsequent fields and values
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_data[0] = 0xd2;
	bytes_in.pkt_data[1] = 0xea;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(DAR_UTIL_INVALID_TT, rc);
	assert_int_equal(0x34, fields_out.phys.pkt_ackID);
	assert_int_equal(0x1, fields_out.phys.pkt_vc);
	assert_int_equal(0x0, fields_out.phys.crf);
	assert_int_equal(0x3, fields_out.phys.pkt_prio);
	assert_int_equal(0x2, fields_out.trans.tt_code);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_src_dst_addr_size_test(void **state)
{
	// ftype = bytes_in->pkt_data[1]
	// tt_code = bytes_in->pkt_data[1] & 0x30
	const uint8_t small_addr[] = {0x12, 0x02, 0xde, 0xad, 0x40};
	const uint8_t large_addr[] = {0x34, 0x12, 0xde, 0xad, 0xbe, 0xef, 0xc0};

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t rc;

	// verify small addresses are correctly parsed
	// ignores subsequent fields and values
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	memcpy(&bytes_in.pkt_data, &small_addr, sizeof(small_addr));

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);

	// verify large addresses are correctly parsed
	// ignores subsequent fields and values
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	memcpy(&bytes_in.pkt_data, &large_addr, sizeof(large_addr));

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xdead, fields_out.trans.destID);
	assert_int_equal(0xbeef, fields_out.trans.srcID);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_2_pkt_type_test(void **state)
{
	const uint8_t input[] = {0x12, 0x02, 0xde, 0xad, 0x44, 0xa1, 0xc2, 0xc4,
			0xc0, 0xa3};
	const uint32_t pkt_type_idx = 4;
	const uint32_t pkt_type_shift = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t expected_rc;
	uint32_t expected_pkt_type;
	uint32_t idx;
	uint32_t rc;

	// verify pkt_type
	// ignore other fields
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_addr_size = rio_addr_34;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	for (idx = 0; idx < 0x10; idx++) {
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		expected_rc = RIO_SUCCESS;
		switch (idx) {
		case 0x4:
			expected_pkt_type = pkt_nr;
			break;
		case 0xc:
			expected_pkt_type = pkt_nr_inc;
			break;
		case 0xd:
			expected_pkt_type = pkt_nr_dec;
			break;
		case 0xe:
			expected_pkt_type = pkt_nr_set;
			break;
		case 0xf:
			expected_pkt_type = pkt_nr_clr;
			break;
		default:
			expected_rc = DAR_UTIL_UNKNOWN_TRANS;
			expected_pkt_type = pkt_raw;
		}

		bytes_in.pkt_data[pkt_type_idx] = (idx << pkt_type_shift);
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(expected_rc, rc);
		assert_int_equal(0xde, fields_out.trans.destID);
		assert_int_equal(0xad, fields_out.trans.srcID);
		assert_int_equal(expected_pkt_type, fields_out.pkt_type);
	}

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_2_memsz_34_test(void **state)
{
	const uint8_t input[] = {0x02, 0x82, 0xde, 0xad, 0x44, 0xa1, 0xde, 0xad,
			0xbe, 0xeb, 0xa9, 0xc0};

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t rc;

	// verify fields for an ftype of 2, and pkt_type of pkt_nr
	// uses an address size of rio_addr_34
	// uses and rdsize of 4
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = rio_addr_34;
	bytes_in.num_chars = sizeof(input);
	bytes_in.pkt_has_crc = true;
	bytes_in.pkt_padded = false;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);

//	assert_int_equal(4, fields_out.phys.pkt_ackID);
	assert_int_equal(0, fields_out.phys.pkt_ackID);
	assert_int_equal(1, fields_out.phys.pkt_vc);
	assert_int_equal(0, fields_out.phys.crf);

	assert_int_equal(2, fields_out.phys.pkt_prio);
	assert_int_equal(tt_small, fields_out.trans.tt_code);
	assert_int_equal(pkt_nr, fields_out.pkt_type);

	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);

	assert_int_equal(0xa1, fields_out.log_rw.tid);
	assert_int_equal(rio_addr_34, fields_out.log_rw.pkt_addr_size);
	assert_int_equal(0xdeadbee8, fields_out.log_rw.addr[0]);
	assert_int_equal(0x3, fields_out.log_rw.addr[1]);
	assert_int_equal(0x0, fields_out.log_rw.addr[2]);

	assert_int_equal(2, fields_out.pkt_bytes);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_2_memsz_50_test(void **state)
{
	const uint8_t input[] = {0x01, 0x52, 0xde, 0xad, 0xbe, 0xef, 0x44, 0x1a,
			0x00, 0x03, 0xca, 0xfe, 0xba, 0xbc, 0x84, 0xd1};

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t rc;

	// verify fields for an ftype of 2, and pkt_type of pkt_nr_dec
	// uses an address size of rio_addr_50
	// uses and rdsize of 6
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = rio_addr_50;
	bytes_in.num_chars = sizeof(input);
	bytes_in.pkt_has_crc = true;
	bytes_in.pkt_padded = false;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);

//	assert_int_equal(2, fields_out.phys.pkt_ackID);
	assert_int_equal(0, fields_out.phys.pkt_ackID);
	assert_int_equal(0, fields_out.phys.pkt_vc);
	assert_int_equal(1, fields_out.phys.crf);

	assert_int_equal(1, fields_out.phys.pkt_prio);
	assert_int_equal(tt_large, fields_out.trans.tt_code);
	assert_int_equal(pkt_nr, fields_out.pkt_type);

	assert_int_equal(0xdead, fields_out.trans.destID);
	assert_int_equal(0xbeef, fields_out.trans.srcID);

	assert_int_equal(0x1a, fields_out.log_rw.tid);
	assert_int_equal(rio_addr_50, fields_out.log_rw.pkt_addr_size);
	assert_int_equal(0xcafebabc, fields_out.log_rw.addr[0]);
	assert_int_equal(0x3, fields_out.log_rw.addr[1]);
	assert_int_equal(0x0, fields_out.log_rw.addr[2]);

	assert_int_equal(2, fields_out.pkt_bytes);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_5_pkt_type_test(void **state)
{
	const uint8_t input[] = {0x12, 0x05, 0xde, 0xad, 0x54, 0xa1, 0xc2, 0xc4,
			0xc0, 0xa3};
	const uint32_t pkt_type_idx = 4;
	const uint32_t pkt_type_shift = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t expected_rc;
	uint32_t expected_pkt_type;
	uint32_t idx;
	uint32_t rc;

	// verify pkt_type
	// ignore other fields
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_addr_size = rio_addr_34;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	for (idx = 0; idx < 0x10; idx++) {
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		fields_out.pkt_data = pkt_data;
		expected_rc = RIO_SUCCESS;
		switch (idx) {
		case 0x4:
			expected_pkt_type = pkt_nw;
			break;
		case 0x5:
			expected_pkt_type = pkt_nwr;
			break;
		case 0xc:
			expected_pkt_type = pkt_nw_swap;
			break;
		case 0xd:
			expected_pkt_type = pkt_nw_cmp_swap;
			break;
		case 0xe:
			expected_pkt_type = pkt_nw_tst_swap;
			break;
		default:
			expected_rc = DAR_UTIL_UNKNOWN_TRANS;
			expected_pkt_type = 0;
		}

		bytes_in.pkt_data[pkt_type_idx] = (idx << pkt_type_shift);
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(expected_rc, rc);
		assert_int_equal(0xde, fields_out.trans.destID);
		assert_int_equal(0xad, fields_out.trans.srcID);
		assert_int_equal(expected_pkt_type, fields_out.pkt_type);
	}

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_5_memsz_66_test(void **state)
{
	const uint8_t input[] = {0x02, 0xd5, 0xca, 0xfe, 0xba, 0xbe, 0x54, 0x67,
			0xa5, 0x5a, 0xa5, 0x5a, 0xde, 0xad, 0xbe, 0xe8, 0xa5,
			0xa5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf0, 0x91};

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	// verify fields for an ftype of 2, and pkt_type of pkt_nr_dec
	// uses an address size of rio_addr_50
	// uses and rdsize of 6
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0, sizeof(pkt_data));
	fields_out.pkt_data = pkt_data;

	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = rio_addr_66;
	bytes_in.num_chars = sizeof(input);
	bytes_in.pkt_has_crc = true;
	bytes_in.pkt_padded = true;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);

//	assert_int_equal(1, fields_out.phys.pkt_ackID);
	assert_int_equal(0, fields_out.phys.pkt_ackID);
	assert_int_equal(1, fields_out.phys.pkt_vc);
	assert_int_equal(0, fields_out.phys.crf);

	assert_int_equal(3, fields_out.phys.pkt_prio);
	assert_int_equal(tt_large, fields_out.trans.tt_code);
	assert_int_equal(pkt_nwr, fields_out.pkt_type);

	assert_int_equal(0xcafe, fields_out.trans.destID);
	assert_int_equal(0xbabe, fields_out.trans.srcID);

	assert_int_equal(0x67, fields_out.log_rw.tid);
	assert_int_equal(rio_addr_66, fields_out.log_rw.pkt_addr_size);
	assert_int_equal(0xdeadbee8, fields_out.log_rw.addr[0]);
	assert_int_equal(0xa55aa55a, fields_out.log_rw.addr[1]);
	assert_int_equal(0x0, fields_out.log_rw.addr[2]);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_6_pkt_type_test(void **state)
{
	// ftype = bytes_in->pkt_data[1]
	// pkt_type = bytes_in->pkt_data[4] & 0xF0) >> 4) (small addr)
	const uint8_t input[] = {0x12, 0x06, 0xde, 0xad, 0x64, 0xa1, 0xc2, 0xc4,
			0xc0, 0xa3};
	const uint32_t pkt_type_idx = 4;
	const uint32_t pkt_type_shift = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t idx;
	uint32_t rc;

	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0, sizeof(pkt_data));
	fields_out.pkt_data = pkt_data;

	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_addr_size = rio_addr_34;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	for (idx = 0; idx < 0x10; idx++) {
		bytes_in.pkt_data[pkt_type_idx] = (idx << pkt_type_shift);
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_int_equal(0xde, fields_out.trans.destID);
		assert_int_equal(0xad, fields_out.trans.srcID);
		assert_int_equal(pkt_sw, fields_out.pkt_type);
	}

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_6_memsz_32_test(void **state)
{
	const uint8_t input[] = {0x2, 0x46, 0xca, 0xfe, 0xde, 0xad, 0xbe, 0xe8,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x52, 0xc1};

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	// verify fields for an ftype of 6, and pkt_type of pkt_sw
	// uses an address size of rio_addr_32
	// uses and rdsize of 6
	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0x5a, sizeof(pkt_data));
	fields_out.pkt_data = pkt_data;

	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_addr_size = rio_addr_32;
	bytes_in.num_chars = sizeof(input);
	bytes_in.pkt_has_crc = true;
	bytes_in.pkt_padded = true;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);

//	assert_int_equal(1, fields_out.phys.pkt_ackID);
	assert_int_equal(0, fields_out.phys.pkt_ackID);
	assert_int_equal(1, fields_out.phys.pkt_vc);
	assert_int_equal(0, fields_out.phys.crf);

	assert_int_equal(1, fields_out.phys.pkt_prio);
	assert_int_equal(tt_small, fields_out.trans.tt_code);
	assert_int_equal(pkt_sw, fields_out.pkt_type);

	assert_int_equal(0xca, fields_out.trans.destID);
	assert_int_equal(0xfe, fields_out.trans.srcID);

	assert_int_equal(0x0, fields_out.log_rw.tid);
	assert_int_equal(rio_addr_32, fields_out.log_rw.pkt_addr_size);
	assert_int_equal(0xdeadbee8, fields_out.log_rw.addr[0]);
	assert_int_equal(0x0, fields_out.log_rw.addr[1]);
	assert_int_equal(0x0, fields_out.log_rw.addr[2]);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_7_pkt_type_test(void **state)
{
	// ftype = bytes_in->pkt_data[1]
	// pkt_type = bytes_in->pkt_data[4] & 0xF0) >> 4) (small addr)
	const uint8_t input[] = {0x12, 0x07, 0xde, 0xad, 0x64, 0xa1, 0xc2};

	const uint32_t pkt_type_idx = 4;
	const uint32_t pkt_type_shift = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t idx;
	uint32_t rc;

	// verify pkt_type and all other fields
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_addr_size = rio_addr_34;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	for (idx = 0; idx < 0x10; idx++) {
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		bytes_in.pkt_data[pkt_type_idx] = (idx << pkt_type_shift);
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(RIO_SUCCESS, rc);
		assert_int_equal(0xde, fields_out.trans.destID);
		assert_int_equal(0xad, fields_out.trans.srcID);
		assert_int_equal(pkt_fc, fields_out.pkt_type);
		if (idx < 8) {
			assert_false(fields_out.log_fc.fc_xon);
		} else {
			assert_true(fields_out.log_fc.fc_xon);
		}
		assert_int_equal(idx % 8, fields_out.log_fc.fc_fam);
		assert_int_equal(0x50, fields_out.log_fc.fc_flow);
		assert_true(fields_out.log_fc.fc_soc_is_ep);
		assert_int_equal(0xde, fields_out.log_fc.fc_destID);
		assert_int_equal(0xad, fields_out.log_fc.fc_srcID);
	}

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_8_memsz_21_test(void **state)
{
	// ftype = bytes_in->pkt_data[1 & 0x0F]
	// pkt_type = bytes_in->pkt_data[4] & 0xF0) >> 4) (small addr)
	const uint8_t input[] = {0x12, 0x08, 0xde, 0xad, 0x44, 0xa1, 0xc2, 0xca,
			0xfe, 0xba};
	const uint32_t pkt_type_idx = 4;
	const uint32_t pkt_type_shift = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t expected_rc;
	uint32_t expected_pkt_type;
	uint32_t expected_addr;
	uint32_t expected_pkt_bytes;
	uint32_t idx;
	uint32_t rc;

	// verify pkt_type
	// ignore other fields
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_addr_size = rio_addr_21;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	for (idx = 0; idx < 0x10; idx++) {
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		fields_out.pkt_data = pkt_data;

		expected_rc = RIO_SUCCESS;
		expected_pkt_bytes = 0;
		switch (idx) {
		case 0:
			expected_pkt_type = pkt_mr;
			expected_addr = 0xcafeb8;
			expected_pkt_bytes = 1;
			break;
		case 1:
			expected_pkt_type = pkt_mw;
			expected_addr = 0xcafeb8;
			expected_pkt_bytes = 1;
			break;
		case 2:
			expected_pkt_type = pkt_mrr;
			expected_addr = 0xcafeb8;
			break;
		case 3:
			expected_pkt_type = pkt_mwr;
			expected_addr = 0xcafeb8;
			break;
		case 4:
			expected_pkt_type = pkt_pw;
			expected_addr = 0xcafeb8;
			expected_pkt_bytes = 1;
			break;
		default:
			expected_rc = DAR_UTIL_UNKNOWN_TRANS;
			expected_pkt_type = pkt_raw;
			expected_addr = 0x0;
		}

		bytes_in.pkt_addr_size = rio_addr_21;
		bytes_in.pkt_data[pkt_type_idx] = (idx << pkt_type_shift);
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(expected_rc, rc);
		assert_int_equal(0xde, fields_out.trans.destID);
		assert_int_equal(0xad, fields_out.trans.srcID);
		assert_int_equal(expected_pkt_type, fields_out.pkt_type);
		assert_int_equal(expected_addr, fields_out.log_rw.addr[0]);
		assert_int_equal(0x0, fields_out.log_rw.addr[1]);
		assert_int_equal(0x0, fields_out.log_rw.addr[2]);
		assert_int_equal(expected_pkt_bytes, fields_out.pkt_bytes);
		if ((idx > 4)) {
			assert_int_equal(rio_addr_21, bytes_in.pkt_addr_size);
			assert_int_equal(0, fields_out.log_rw.tid);
			assert_int_equal(0, fields_out.trans.hopcount);
			assert_int_equal(0, fields_out.log_rw.pkt_addr_size);
		} else {
			assert_int_equal(rio_addr_21, bytes_in.pkt_addr_size);
			assert_int_equal(0xa1, fields_out.log_rw.tid);
			assert_int_equal(0xc2, fields_out.trans.hopcount);
			assert_int_equal(rio_addr_21,
					fields_out.log_rw.pkt_addr_size);
		}
	}

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_9_pkt_type_test(void **state)
{
	// ftype = bytes_in->pkt_data[1 & 0x0F]
	const uint8_t input[] = {0x12, 0x09, 0xde, 0xad, 0x44, 0xa1, 0xca, 0xfe,
			0xba, 0xde, 0xad, 0xbe, 0xef};

	const uint32_t ext_idx = 5;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint32_t rc;

	// extended bit not set
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_addr_size = rio_addr_34;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);
	assert_int_equal(pkt_dstm, fields_out.pkt_type);
	assert_int_equal(0x44, fields_out.log_ds.dstm_COS);

	assert_false(fields_out.log_ds.dstm_xh_seg);
	assert_int_equal(0, fields_out.log_ds.dstm_xh_type);
	assert_int_equal(0, fields_out.log_ds.dstm_xh_tm_op);
	assert_int_equal(0, fields_out.log_ds.dstm_xh_wildcard);
	assert_int_equal(0, fields_out.log_ds.dstm_xh_COS_mask);
	assert_int_equal(0, fields_out.log_ds.dstm_xh_parm1);
	assert_int_equal(0, fields_out.log_ds.dstm_xh_parm2);
	assert_true(fields_out.log_ds.dstm_start_seg);
	assert_false(fields_out.log_ds.dstm_end_seg);
	assert_false(fields_out.log_ds.dstm_odd_data_amt);
	assert_true(fields_out.log_ds.dstm_pad_data_amt);
	assert_int_equal(0xcafe, fields_out.log_ds.dstm_streamid);
	assert_int_equal(0, fields_out.log_ds.dstm_PDU_len);

	// extended bit set
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	bytes_in.pkt_addr_size = rio_addr_34;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_data[ext_idx] = 0x27;

	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);
	assert_int_equal(pkt_dstm, fields_out.pkt_type);
	assert_int_equal(0x44, fields_out.log_ds.dstm_COS);

	assert_true(fields_out.log_ds.dstm_xh_seg);
	assert_false(fields_out.log_ds.dstm_start_seg);
	assert_false(fields_out.log_ds.dstm_end_seg);
	assert_false(fields_out.log_ds.dstm_odd_data_amt);
	assert_false(fields_out.log_ds.dstm_pad_data_amt);
	assert_int_equal(4, fields_out.log_ds.dstm_xh_type);
	assert_int_equal(0xcafe, fields_out.log_ds.dstm_streamid);
	assert_int_equal(0, fields_out.log_ds.dstm_PDU_len);
	assert_int_equal(0xb, fields_out.log_ds.dstm_xh_tm_op);
	assert_int_equal(2, fields_out.log_ds.dstm_xh_wildcard);
	assert_int_equal(2, fields_out.log_ds.dstm_xh_wildcard);
	assert_int_equal(0xde, fields_out.log_ds.dstm_xh_COS_mask);
	assert_int_equal(0xad, fields_out.log_ds.dstm_xh_parm1);
	assert_int_equal(0xbe, fields_out.log_ds.dstm_xh_parm2);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_10_pkt_type_test(void **state)
{
	// ftype = bytes_in->pkt_data[1 & 0x0F]
	const uint8_t input[] = {0x12, 0x0a, 0xde, 0xad, 0x44, 0xa1, 0xca, 0xfe,
			0xba, 0xbe};

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint8_t out[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	// verify all fields
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(pkt_data, 0xa5, sizeof(pkt_data));
	fields_out.pkt_data = pkt_data;

	memset(out, 0xa5, sizeof(pkt_data));
	out[0] = 0xca;
	out[1] = 0xfe;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);
	assert_int_equal(pkt_db, fields_out.pkt_type);
	assert_int_equal(0xa1, fields_out.log_rw.tid);
	assert_int_equal(0xca, fields_out.pkt_data[0]);
	assert_int_equal(0xfe, fields_out.pkt_data[1]);
	assert_int_equal(2, fields_out.pkt_bytes);
	assert_memory_equal(out, fields_out.pkt_data, sizeof(out));

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_11_pkt_type_test(void **state)
{
	// ftype = bytes_in->pkt_data[1 & 0x0F]
	const uint8_t input[] = {0x12, 0x0b, 0xde, 0xad, 0xae, 0x5a, 0x44, 0x88,
			0xba, 0xbe};
	const uint32_t length_idx = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t idx;
	uint32_t rc;

	// verify all fields
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(pkt_data, 0xa5, sizeof(pkt_data));
	fields_out.pkt_data = pkt_data;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);
	assert_int_equal(pkt_msg, fields_out.pkt_type);
	assert_int_equal(0xa, fields_out.log_ms.msg_len);
	assert_int_equal(1, fields_out.log_ms.letter);

	if (fields_out.log_ms.msg_len) {
		assert_int_equal(1, fields_out.log_ms.mbid);
		assert_int_equal(0xa, fields_out.log_ms.msgseg);
	} else {
		assert_int_equal(0, fields_out.log_ms.mbid);
		assert_int_equal(0, fields_out.log_ms.msgseg);
	}

	// verify failure for rdsize < 9
	for (idx = 0; idx < 9; idx++) {
		bytes_in.pkt_data[length_idx] = (0xa0 | idx);
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(DAR_UTIL_BAD_MSG_DSIZE, rc);
	}

	bytes_in.pkt_data[length_idx] = 0xaa;
	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_13_pkt_type_test(void **state)
{
	// ftype = bytes_in->pkt_data[1 & 0x0F]
	// pkt_type = bytes_in->pkt_data[4] & 0xF0) >> 4) (small addr)
	const uint8_t input[] = {0x12, 0x0d, 0xde, 0xad, 0x44, 0x5a, 0xca, 0xfe,
			0xba, 0xbe};
	const uint32_t pkt_type_idx = 4;
	const uint32_t pkt_type_shift = 4;

	DAR_pkt_bytes_t bytes_in;
	DAR_pkt_fields_t fields_out;
	uint8_t out[RIO_MAX_PKT_BYTES];

	uint32_t expected_rc;
	uint32_t expected_pkt_type;
	uint32_t expected_status;
	uint32_t idx;
	uint32_t idx2;
	uint32_t rc;

	// verify all fields
	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	bytes_in.num_chars = TST_MIN_NUM_CHARS;
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));

	for (idx = 0; idx < 0x10; idx++) {
		memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
		memset(out, 0, sizeof(out));
		fields_out.pkt_data = out;
		expected_rc = RIO_SUCCESS;

		switch (idx) {
		case 0:
			expected_pkt_type = pkt_resp;
			break;
		case 1:
			expected_pkt_type = pkt_msg_resp;
			break;
		case 8:
			expected_pkt_type = pkt_resp_data;
			break;
		default:
			expected_pkt_type = 0;
			expected_rc = DAR_UTIL_UNKNOWN_TRANS;
		}

		bytes_in.pkt_data[pkt_type_idx] = (idx << pkt_type_shift);
		rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
		assert_int_equal(expected_rc, rc);
		assert_int_equal(0xde, fields_out.trans.destID);
		assert_int_equal(0xad, fields_out.trans.srcID);
		assert_int_equal(expected_pkt_type, fields_out.pkt_type);

		if (DAR_UTIL_UNKNOWN_TRANS == rc) {
			continue;
		}

		for (idx2 = 0; idx2 < 0x10; idx2++) {
			memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
			memset(out, 0, sizeof(out));
			fields_out.pkt_data = out;
			expected_rc = RIO_SUCCESS;

			switch (idx2) {
			case 0:
				expected_status = pkt_done;
				break;
			case 3:
				expected_status = pkt_retry;
				break;
			case 7:
				expected_status = pkt_err;
				break;
			default:
				expected_status = 0;
				expected_rc = DAR_UTIL_UNKNOWN_STATUS;
			}

			bytes_in.pkt_data[pkt_type_idx] = ((idx
					<< pkt_type_shift) | idx2);
			rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
			assert_int_equal(expected_rc, rc);
			assert_int_equal(expected_status,
					fields_out.log_ms.status);
			assert_int_equal(expected_status,
					fields_out.log_rw.status);
		}

		if (DAR_UTIL_UNKNOWN_STATUS == rc) {
			continue;
		}

		if (pkt_msg_resp == fields_out.pkt_type) {
			assert_int_equal(1, fields_out.log_ms.letter);
			assert_int_equal(2, fields_out.log_ms.mbid);
			assert_int_equal(0, fields_out.log_ms.msgseg);
			assert_int_equal(0, fields_out.log_rw.tid);
		} else {
			assert_int_equal(0, fields_out.log_ms.letter);
			assert_int_equal(0, fields_out.log_ms.mbid);
			assert_int_equal(0, fields_out.log_ms.msgseg);
			assert_int_equal(0x5a, fields_out.log_rw.tid);
		}
	}

	(void)state; // unused
}

static void DAR_pkt_bytes_to_fields_ftype_raw_pkt_type_test(void **state)
{
	const uint8_t input[] = {0x12, 0x00, 0xde, 0xad, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0x34, 0x3d};

	// INFW
	DAR_pkt_fields_t fields_out;
	DAR_pkt_bytes_t bytes_in;
	uint8_t pkt_data[RIO_MAX_PKT_BYTES];
	uint32_t rc;

	memset(&fields_out, 0, sizeof(DAR_pkt_fields_t));
	memset(&pkt_data, 0, sizeof(pkt_data));
	fields_out.pkt_data = pkt_data;

	memset(&bytes_in, 0, sizeof(DAR_pkt_bytes_t));
	memcpy(&bytes_in.pkt_data, &input, sizeof(input));
	bytes_in.pkt_has_crc = true;
	bytes_in.pkt_padded = true;
	bytes_in.num_chars = 12;
	bytes_in.pkt_addr_size = rio_addr_32;

	rc = DAR_pkt_bytes_to_fields(&bytes_in, &fields_out);
	assert_int_equal(RIO_SUCCESS, rc);
	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);
	assert_int_equal(pkt_raw, fields_out.pkt_type);

	assert_int_equal(0, fields_out.tot_bytes);
	assert_int_equal(0, fields_out.pad_bytes);

	assert_int_equal(4, fields_out.phys.pkt_ackID);
	assert_int_equal(1, fields_out.phys.pkt_vc);
	assert_int_equal(0, fields_out.phys.crf);
	assert_int_equal(0, fields_out.phys.pkt_prio);

	assert_int_equal(tt_small, fields_out.trans.tt_code);
	assert_int_equal(0xde, fields_out.trans.destID);
	assert_int_equal(0xad, fields_out.trans.srcID);
	assert_int_equal(0, fields_out.trans.hopcount);

	assert_int_equal(pkt_raw, fields_out.pkt_type);
	assert_int_equal(10, fields_out.pkt_bytes);

	assert_memory_equal(&bytes_in.pkt_data, &pkt_data, 10);

	(void)state; // unused
}

static void DAR_pkt_ftype_descr_test(void **state)
{
	// highlights if a string changes value
	DAR_pkt_fields_t pkt_fields;
	uint32_t idx;

	for (idx = pkt_type_min; idx <= pkt_type_max; idx++) {
		pkt_fields.pkt_type = (DAR_pkt_type)idx;
		switch (idx) {
		case pkt_raw:
			assert_string_equal("RAW",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_nr):
		case (pkt_nr_inc):
		case (pkt_nr_dec):
		case (pkt_nr_set):
		case (pkt_nr_clr):
			assert_string_equal("NRead",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_nw):
		case (pkt_nwr):
		case (pkt_nw_swap):
		case (pkt_nw_cmp_swap):
		case (pkt_nw_tst_swap):
			assert_string_equal("NWrite",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_sw):
			assert_string_equal("SWrite",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_fc):
			assert_string_equal("Flow Control",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_mr):
		case (pkt_mw):
		case (pkt_mrr):
		case (pkt_mwr):
		case (pkt_pw):
			assert_string_equal("Maintenance",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_dstm):
			assert_string_equal("Data Streaming",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_db):
			assert_string_equal("Doorbell",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_msg):
			assert_string_equal("Message",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		case (pkt_resp):
		case (pkt_resp_data):
		case (pkt_msg_resp):
			assert_string_equal("Response",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		default:
			assert_string_equal("Reserved",
					(DAR_pkt_ftype_descr(&pkt_fields)));
			break;
		}
	}

	(void)state; // unused
}

static void DAR_pkt_trans_descr_test(void **state)
{
	// highlights if a string changes value
	DAR_pkt_fields_t pkt_fields;
	uint32_t idx;

	for (idx = pkt_type_min; idx <= pkt_type_max; idx++) {
		pkt_fields.pkt_type = (DAR_pkt_type)idx;
		switch (idx) {
		case pkt_raw:
			assert_string_equal("",
					DAR_pkt_trans_descr(&pkt_fields));
			break;
		case (pkt_nr):
			assert_string_equal("NREAD",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_nr_inc):
			assert_string_equal("ATOMIC inc",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_nr_dec):
			assert_string_equal("ATOMIC dec",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_nr_set):
			assert_string_equal("ATOMIC set",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_nr_clr):
			assert_string_equal("ATOMIC clr",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_nw):
			assert_string_equal("NWRITE",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_nwr):
			assert_string_equal("NWRITE_R",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_nw_swap):
			assert_string_equal("ATOMIC swap",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;

		case (pkt_nw_cmp_swap):
			assert_string_equal("ATOMIC cmp swap",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;

		case (pkt_nw_tst_swap):
			assert_string_equal("ATOMIC tst swap",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_sw):
		case (pkt_fc):
			assert_string_equal("",
					DAR_pkt_trans_descr(&pkt_fields));
			break;
		case (pkt_mr):
			assert_string_equal("MtcRead",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_mw):
			assert_string_equal("MtcWrite",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;

		case (pkt_mrr):
			assert_string_equal("MtcReadResp",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;

		case (pkt_mwr):
			assert_string_equal("MtcWriteResp",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;

		case (pkt_pw):
			assert_string_equal("Port-Write",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_dstm):
		case (pkt_db):
		case (pkt_msg):
		case (pkt_resp):
			assert_string_equal("",
					DAR_pkt_trans_descr(&pkt_fields));
			break;
		case (pkt_resp_data):
			assert_string_equal("Resp with Data",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		case (pkt_msg_resp):
			assert_string_equal("Msg Response",
					(DAR_pkt_trans_descr(&pkt_fields)));
			break;
		default:
			assert_string_equal("",
					DAR_pkt_trans_descr(&pkt_fields));
			break;
		}
	}

	(void)state; // unused
}

static void DAR_pkt_resp_status_descr_test(void **state)
{
	DAR_pkt_fields_t pkt_fields;
	uint32_t i, j;

	for (i = pkt_type_min; i <= pkt_type_max; i++) {
		pkt_fields.pkt_type = (DAR_pkt_type)i;
		switch (i) {
		case pkt_mrr:
		case pkt_mwr:
		case pkt_resp:
		case pkt_msg_resp:
			continue;
		default:
			assert_string_equal("Unknown",
					DAR_pkt_resp_status_descr(&pkt_fields));
		}
	}

	for (i = pkt_type_min; i <= pkt_type_max; i++) {
		pkt_fields.pkt_type = (DAR_pkt_type)i;
		switch (i) {
		case pkt_mrr:
		case pkt_mwr:
		case pkt_resp:
		case pkt_msg_resp:
			break;
		default:
			continue;
		}

		for (j = rio_pkt_status_min; j <= rio_pkt_status_max; j++) {
			pkt_fields.log_rw.status = (rio_pkt_status)j;
			switch (j) {
			case pkt_done:
				assert_string_equal("Done",
						DAR_pkt_resp_status_descr(
								&pkt_fields));
				break;
			case pkt_retry:
				if ((pkt_resp == pkt_fields.pkt_type)
						|| (pkt_msg_resp
								== pkt_fields.pkt_type)) {
					assert_string_equal("Retry",
							DAR_pkt_resp_status_descr(
									&pkt_fields));
				} else {
					assert_string_equal("Unknown",
							DAR_pkt_resp_status_descr(
									&pkt_fields));
				}
				break;
			case pkt_err:
				assert_string_equal("Error",
						DAR_pkt_resp_status_descr(
								&pkt_fields));
				break;
			default:
				assert_string_equal("Unknown",
						DAR_pkt_resp_status_descr(
								&pkt_fields));
				break;
			}
		}
	}

	(void)state; // unused
}

//static void assumptions(void **state)
//{
//	(void)state; // unused
//}

int main(int argc, char** argv)
{
	(void)argv; // not used
	argc++; // not used

	const struct CMUnitTest tests[] = {
	cmocka_unit_test(assumptions),
	cmocka_unit_test(DAR_util_get_ftype_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_1_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_2_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_3_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_4_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_5_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_6_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_7_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_8_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_16_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_32_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_64_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_96_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_128_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_160_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_192_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_224_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_pkt_bytes_256_test),
	cmocka_unit_test(DAR_util_get_rdsize_wdptr_test),
	cmocka_unit_test(DAR_util_compute_rd_bytes_n_align_wptr_0_test),
	cmocka_unit_test(DAR_util_compute_rd_bytes_n_align_wptr_1_test),
	cmocka_unit_test(DAR_util_compute_rd_bytes_n_align_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_0_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_1_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_2_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_3_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_4_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_5_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_6_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_7_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_8_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_9_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_10_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_11_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_12_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_13_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_14_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_15_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_pkt_bytes_16_test),
	cmocka_unit_test(DAR_util_get_wrsize_wdptr_test),
	cmocka_unit_test(DAR_util_compute_wr_bytes_n_align_wptr_0_test),
	cmocka_unit_test(DAR_util_compute_wr_bytes_n_align_wptr_1_test),
	cmocka_unit_test(DAR_util_compute_wr_bytes_n_align_test),
	cmocka_unit_test(DAR_util_pkt_bytes_init_test),
	cmocka_unit_test(DAR_add_rw_addr_addr_size_21_test),
	cmocka_unit_test(DAR_add_rw_addr_addr_size_32_test),
	cmocka_unit_test(DAR_add_rw_addr_addr_size_34_test),
	cmocka_unit_test(DAR_add_rw_addr_addr_size_50_test),
	cmocka_unit_test(DAR_add_rw_addr_addr_size_66_test),
	cmocka_unit_test(DAR_get_rw_addr_addr_size_21_test),
	cmocka_unit_test(DAR_get_rw_addr_addr_size_32_test),
	cmocka_unit_test(DAR_get_rw_addr_addr_size_34_test),
	cmocka_unit_test(DAR_get_rw_addr_addr_size_50_test),
	cmocka_unit_test(DAR_get_rw_addr_addr_size_66_test),
	cmocka_unit_test(DAR_addr_size_addr_size_21_roundtrip_test),
	cmocka_unit_test(DAR_addr_size_addr_size_32_roundtrip_test),
	cmocka_unit_test(DAR_addr_size_addr_size_34_roundtrip_test),
	cmocka_unit_test(DAR_addr_size_addr_size_50_roundtrip_test),
	cmocka_unit_test(DAR_addr_size_addr_size_66_roundtrip_test),
	cmocka_unit_test(count_bits_test),
	cmocka_unit_test(CS_fields_to_bytes_small_test),
	cmocka_unit_test(CS_fields_to_bytes_large_test),
	cmocka_unit_test(CS_bytes_to_fields_small_test),
	cmocka_unit_test(CS_bytes_to_fields_large_test),
	cmocka_unit_test(get_stype0_descr_test),
	cmocka_unit_test(get_stype0_PNA_cause_parm1_test),
	cmocka_unit_test(get_stype0_LR_port_status_parm1_test),
	cmocka_unit_test(get_stype1_descr_test),
	cmocka_unit_test(get_stype1_lreq_cmd_test),
	cmocka_unit_test(get_stype2_descr_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_2_memsz_34_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_2_memsz_50_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_5_memsz_66_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_6_memsz_32_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_7_pkt_type_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_8_memsz_21_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_9_pkt_type_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_10_pkt_type_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_11_pkt_type_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_13_pkt_type_test),
	cmocka_unit_test(DAR_pkt_fields_to_bytes_ftype_raw_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_invalid_tt_code_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_not_enough_bytes_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_bytes_0_and_1_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_src_dst_addr_size_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_2_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_2_memsz_34_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_2_memsz_50_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_5_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_5_memsz_66_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_6_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_6_memsz_32_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_7_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_8_memsz_21_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_9_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_10_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_11_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_13_pkt_type_test),
	cmocka_unit_test(DAR_pkt_bytes_to_fields_ftype_raw_pkt_type_test),
	cmocka_unit_test(DAR_pkt_ftype_descr_test),
	cmocka_unit_test(DAR_pkt_trans_descr_test),
	cmocka_unit_test(DAR_pkt_resp_status_descr_test),

	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

#ifdef __cplusplus
}
#endif
