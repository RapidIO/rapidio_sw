/*
****************************************************************************
Copyright (c) 2014, Integrated Device Technology Inc.
Copyright (c) 2014, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
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
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <signal.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>

#include <stdarg.h>
#include <setjmp.h>
#include "cmocka.h"

#include "cfg.h"
#include "cfg_private.h"
#include "libcli.h"
#include "liblog.h"
#include "ct_test.h"
#include "did_test.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_SUCCESS	(char *)("test/master_success.cfg")
#define SLAVE_SUCCESS	(char *)("test/slave_success.cfg")
#define TOR_SUCCESS  	(char *)("test/tor_success.cfg")
#define RXS_SUCCESS	(char *)("test/rxs_success.cfg")

static int count = 0;

// If the test files do not exist provide a meaningful message and stop test
static void check_file_exists(const char *filename)
{
	struct stat buffer;

	if (0 != stat(filename, &buffer)) {
		fail_msg("File: %s does not exist, are you in the correct directory?", filename);
	}
}

static int grp_setup(void **state)
{
	rdma_log_init("cfg_test.log", 1);
	g_level = RDMA_LL_OFF;

	(void)state; // unused
	return 0;
}

static int grp_teardown(void **state)
{
	if (0 != count) {
		rdma_log_dump();
	}
	rdma_log_close();

	(void)state; // unused
	return 0;
}

static int setup(void **state)
{
	did_reset();
	ct_reset();

	(void)state; // unused
	return 0;
}

static void cfg_parse_mport_size_test(void **state)
{
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	char *test_dd_mtx_fn = (char *)FMD_DFLT_DD_MTX_FN;
	char *test_dd_fn  = (char *)FMD_DFLT_DD_FN;
	uint8_t mem_sz;
	did_t m_did;
	uint32_t m_cm_port;
	uint32_t m_mode;

	check_file_exists(MASTER_SUCCESS);

	m_did = DID_INVALID_ID;
	m_cm_port = 0xbabe;
	m_mode = 0xdead;
	assert_int_equal(0, cfg_parse_file(MASTER_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode));
	assert_int_equal(0, strncmp(dd_mtx_fn, test_dd_mtx_fn, strlen(dd_mtx_fn)));
	assert_int_equal(0, strncmp(dd_fn, test_dd_fn, strlen(test_dd_fn)));
	assert_int_equal(5, did_get_value(m_did));
	assert_int_equal(dev08_sz, did_get_size(m_did));
	assert_int_equal(FMD_DFLT_MAST_CM_PORT, m_cm_port);
	assert_int_equal(1, m_mode);

	mem_sz = 0xca;
	assert_int_equal(0, cfg_get_mp_mem_sz(0, &mem_sz));
	assert_int_equal(CFG_MEM_SZ_34, mem_sz);

	mem_sz = 0xfe;
	assert_int_equal(0, cfg_get_mp_mem_sz(1, &mem_sz));
	assert_int_equal(CFG_MEM_SZ_50, mem_sz);

	mem_sz = 0xba;
	assert_int_equal(0, cfg_get_mp_mem_sz(2, &mem_sz));
	assert_int_equal (CFG_MEM_SZ_66, mem_sz);

	count++;
	free(cfg);
	(void)state; // unused
}

static void cfg_parse_fail(const char *filename)
{
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	did_t m_did = DID_INVALID_ID;
	uint32_t m_cm_port = 0xbeef;
	uint32_t m_mode = 0xcafe;

	assert_int_not_equal(0, cfg_parse_file((char *)filename, &dd_mtx_fn, &dd_fn, &m_did,
		&m_cm_port, &m_mode));
	free(cfg);
}

static void cfg_parse_fail_no_file_test(void **state)
{
	const char *test_file = "test/parse_fail_0.cfg";

	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_1_test(void **state)
{
	const char *test_file = "test/parse_fail_1.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_2_test(void **state)
{
	const char *test_file = "test/parse_fail_2.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_3_test(void **state)
{
	const char *test_file = "test/parse_fail_3.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_4_test(void **state)
{
	const char *test_file = "test/parse_fail_4.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_5_test(void **state)
{
	const char *test_file = "test/parse_fail_5.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_6_test(void **state)
{
	const char *test_file = "test/parse_fail_6.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_7_test(void **state)
{
	const char *test_file = "test/parse_fail_7.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_8_test(void **state)
{
	const char *test_file = "test/parse_fail_8.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_fail_9_test(void **state)
{
	const char *test_file = "test/parse_fail_9.cfg";

	check_file_exists(test_file);
	cfg_parse_fail(test_file);

	count++;
	(void)state; // unused
}

static void cfg_parse_master_test(void **state)
{
	struct cfg_mport_info mp;
	struct cfg_dev dev;
	int conn_pt;
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	did_t m_did = DID_INVALID_ID;
	uint32_t m_cm_port = 0xbeef;
	uint32_t m_mode = 0xcafe;

	check_file_exists(MASTER_SUCCESS);
	memset(&dev, 0, sizeof(dev));

	assert_int_equal(0, cfg_parse_file(MASTER_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode));

	assert_int_equal(0, cfg_find_mport(0, &mp));
	assert_int_equal(0, mp.num);
	assert_int_equal(0x10005, mp.ct);
	assert_int_equal(1, mp.op_mode);
	assert_int_not_equal(0, cfg_find_mport(3, &mp));

	assert_int_equal(0, cfg_find_dev_by_ct(0x10005, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.max_pw);
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_5p0, dev.ep_pt.ls);

	assert_int_equal(0, cfg_find_dev_by_ct(0x20006, &dev));
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.max_pw);
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_6p25, dev.ep_pt.ls);

	assert_int_equal(0, cfg_find_dev_by_ct(0x30007, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.max_pw);
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_1p25, dev.ep_pt.ls);

	assert_int_equal(0, cfg_find_dev_by_ct(0x40008, &dev));

	assert_int_equal(0, cfg_find_dev_by_ct(0x70009, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[0].max_pw);
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[0].op_pw);
	assert_int_equal(rio_pc_ls_6p25, dev.sw_info.sw_pt[0].ls);

	assert_int_equal(rio_pc_pw_2x, dev.sw_info.sw_pt[1].max_pw);
	assert_int_equal(rio_pc_pw_2x, dev.sw_info.sw_pt[1].op_pw);
	assert_int_equal(rio_pc_ls_5p0, dev.sw_info.sw_pt[1].ls);

	assert_int_equal(rio_pc_pw_2x, dev.sw_info.sw_pt[2].max_pw);
	assert_int_equal(rio_pc_pw_1x, dev.sw_info.sw_pt[2].op_pw);
	assert_int_equal(rio_pc_ls_3p125, dev.sw_info.sw_pt[2].ls);

	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[3].max_pw);
	assert_int_equal(rio_pc_pw_1x, dev.sw_info.sw_pt[3].op_pw);
	assert_int_equal(rio_pc_ls_2p5, dev.sw_info.sw_pt[3].ls);

	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[4].max_pw);
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[4].op_pw);
	assert_int_equal(rio_pc_ls_1p25, dev.sw_info.sw_pt[4].ls);

	assert_int_equal(0, cfg_get_conn_dev(0x70009, 0, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 1, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 4, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 5, &dev, &conn_pt));
	assert_int_not_equal(0, cfg_get_conn_dev(0x70009, 7, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x10005, 0, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x20006, 0, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x30007, 0, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x40008, 0, &dev, &conn_pt));
	assert_int_not_equal(0, cfg_get_conn_dev(0x40008, 1, &dev, &conn_pt));
	assert_int_not_equal(0,cfg_find_dev_by_ct(0x99999, &dev));

	count++;
	free(cfg);
	(void)state; // unused
}
	
static void cfg_parse_slave_test(void **state)
{
	struct cfg_mport_info mp;
	struct cfg_dev dev;
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	char *test_dd_mtx_fn = (char *)FMD_DFLT_DD_MTX_FN;
	char *test_dd_fn = (char *)FMD_DFLT_DD_FN;
	did_t m_did = DID_INVALID_ID;
	uint32_t m_cm_port = 0xbabe;
	uint32_t m_mode = 0xdead;

	check_file_exists(SLAVE_SUCCESS);
	memset(&dev, 0, sizeof(dev));

	assert_int_equal(0, cfg_parse_file(SLAVE_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode));
	assert_int_equal(0, strncmp(dd_mtx_fn, test_dd_mtx_fn, strlen(dd_mtx_fn)));
	assert_int_equal(0, strncmp(dd_fn, test_dd_fn, strlen(test_dd_fn)));
	assert_int_equal(5, did_get_value(m_did));
	assert_int_equal(dev08_sz, did_get_size(m_did));
	assert_int_equal(FMD_DFLT_MAST_CM_PORT,m_cm_port);
	assert_int_equal(0, m_mode);

	assert_int_equal(0, cfg_find_mport(0, &mp));
	assert_int_not_equal(0, cfg_find_mport(3, &mp));
	assert_int_equal(0, cfg_find_dev_by_ct(0x20006, &dev));
	assert_int_not_equal(0, cfg_find_dev_by_ct(0x70000, &dev));

	count++;
	free(cfg);
	(void)state; // unused
}
	
static void cfg_parse_tor_test(void **state)
{
	struct cfg_mport_info mp;
	struct cfg_dev dev;
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	char *test_dd_mtx_fn = (char *)FMD_DFLT_DD_MTX_FN;
	char *test_dd_fn = (char *)FMD_DFLT_DD_FN;
	did_t m_did = DID_INVALID_ID;
	uint32_t m_cm_port = 0xbabe;
	uint32_t m_mode = 0xdead;
	int p_idx, idx;
	int pnums[6] = {2, 3, 5, 6, 10, 11};
	uint8_t chk_pnum[6] = {0, 1, 4, 7, 8, 9};

	check_file_exists(TOR_SUCCESS);
	memset(&dev, 0, sizeof(dev));

	assert_int_equal(0, cfg_parse_file(TOR_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode));

	assert_int_equal(0, strncmp(dd_mtx_fn, test_dd_mtx_fn, strlen(dd_mtx_fn)));
	assert_int_equal(0, strncmp(dd_fn, test_dd_fn, strlen(test_dd_fn)));
	assert_int_equal(0x1a, did_get_value(m_did));
	assert_int_equal(dev08_sz, did_get_size(m_did));

	assert_int_equal(FMD_DFLT_MAST_CM_PORT, m_cm_port);
	assert_int_equal(1, m_mode);

	assert_int_equal(0, cfg_find_mport(0, &mp));
	assert_int_not_equal(0, cfg_find_mport(3, &mp));
	assert_int_equal(0, cfg_find_dev_by_ct(0x21001A, &dev));
	assert_int_equal(0, cfg_find_dev_by_ct(0x700F7, &dev));

	/* Check out the switch routing table parsing in detail. */
	assert_int_equal(0, cfg_find_dev_by_ct(0x100f1, &dev));
	assert_int_equal(1, dev.is_sw);
	assert_int_equal(RIO_RTE_DROP, dev.sw_info.rt[CFG_DEV08]->default_route);
	assert_int_equal(2, dev.sw_info.rt[CFG_DEV08]->dev_table[0x12].rte_val);
	assert_int_equal(3, dev.sw_info.rt[CFG_DEV08]->dev_table[0x13].rte_val);
	assert_int_equal(5, dev.sw_info.rt[CFG_DEV08]->dev_table[0x15].rte_val);
	assert_int_equal(6, dev.sw_info.rt[CFG_DEV08]->dev_table[0x16].rte_val);
	assert_int_equal(10, dev.sw_info.rt[CFG_DEV08]->dev_table[0x1A].rte_val);
	assert_int_equal(11, dev.sw_info.rt[CFG_DEV08]->dev_table[0x1B].rte_val);

	for (p_idx = 0; p_idx < 6; p_idx++) {
		int pnum = pnums[p_idx];
		for (idx = 0; idx <= RIO_LAST_DEV8; idx++) {
			assert_int_equal(chk_pnum[p_idx], dev.sw_info.sw_pt[pnum].rt[CFG_DEV08]->dev_table[idx].rte_val);
		}
	}

	/* Check out connection parsing between endpoints & switches,
	* and between switches.
	*/
	for (int idx = 0; idx < 4; idx++) {
		struct cfg_dev ep, sw, rev_ep;
		int sw_pt, rev_pt;
		uint32_t ct[4] = { 0x21001A, 0x220015, 0x230012, 0x240013 };

		assert_int_equal(0, cfg_find_dev_by_ct(ct[idx], &ep));
		assert_int_equal(0, cfg_get_conn_dev(ct[idx], 0, &sw, &sw_pt));
		assert_int_equal(0, cfg_get_conn_dev(sw.ct, sw_pt, &rev_ep, &rev_pt));
		assert_int_equal(0, memcmp(&ep, &rev_ep, sizeof(ep)));
		assert_int_equal(0, rev_pt);
	}

	for (int sw = 1; sw < 7; sw++) {
		struct cfg_dev l0_sw, l1_sw, rev_dev;
		uint32_t ct = 0x000f0 + (0x10001 * sw);
		int port_list[6] = {0, 1, 4, 7, 8, 9};
		int pt_idx;
		int l0_pt, l1_pt, rev_pt;

		assert_int_equal(0, cfg_find_dev_by_ct(ct, &l0_sw));
		for (pt_idx = 0; pt_idx < 6; pt_idx++) {
			l0_pt = port_list[pt_idx];

			assert_int_equal(0, cfg_get_conn_dev(ct, l0_pt, &l1_sw, &l1_pt));
			assert_int_equal(0, cfg_get_conn_dev(l1_sw.ct, l1_pt, &rev_dev, &rev_pt));
			assert_int_equal(0, memcmp(&l0_sw, &rev_dev, sizeof(l0_sw)));
			assert_int_equal(l0_pt, rev_pt);
		}
	}

	count++;
	free(cfg);
	(void)state; // unused
}

static void cfg_parse_rxs_test(void **state)
{
	struct cfg_dev dev;
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	char *test_dd_mtx_fn = (char *)FMD_DFLT_DD_MTX_FN;
	char *test_dd_fn = (char *)FMD_DFLT_DD_FN;
	did_t m_did = DID_INVALID_ID;
	uint32_t m_cm_port = 0xbeef;
	uint32_t m_mode = 0xcafe;
	int conn_pt;

	check_file_exists(RXS_SUCCESS);
	memset(&dev, 0, sizeof(dev));

	assert_int_equal(0, cfg_parse_file(RXS_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
		&m_cm_port, &m_mode));
	assert_int_equal(0, strncmp(dd_mtx_fn, test_dd_mtx_fn, strlen(dd_mtx_fn)));
	assert_int_equal(0, strncmp(dd_fn, test_dd_fn, strlen(test_dd_fn)));
	assert_int_equal(5, did_get_value(m_did));
	assert_int_equal(dev08_sz, did_get_size(m_did));
	assert_int_equal(FMD_DFLT_MAST_CM_PORT, m_cm_port);

	assert_int_equal(0, cfg_find_dev_by_ct(0x10005, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.max_pw);
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_5p0, dev.ep_pt.ls);

	assert_int_equal(0, cfg_find_dev_by_ct(0x20006, &dev));
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.max_pw);
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_6p25, dev.ep_pt.ls);

	assert_int_equal(0, cfg_find_dev_by_ct(0x30007, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.max_pw);
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_1p25, dev.ep_pt.ls);

	assert_int_equal(0, cfg_find_dev_by_ct(0x40008, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.max_pw);
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_2p5, dev.ep_pt.ls);

	assert_int_equal(0, cfg_find_dev_by_ct(0x70000, &dev));
	assert_int_equal(0, memcmp("RXS2448", dev.dev_type, sizeof("RXS2448")));
	assert_int_equal(0,cfg_get_conn_dev(0x70000, 0, &dev, &conn_pt));

	count++;
	free(cfg);
	(void)state; // unused
}


int main(int argc, char *argv[])
{
	(void)argv; // not used
	argc++; // not used

	const struct CMUnitTest tests[] = {
	cmocka_unit_test_setup(cfg_parse_mport_size_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_no_file_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_1_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_2_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_3_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_4_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_5_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_6_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_7_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_8_test, setup),
	cmocka_unit_test_setup(cfg_parse_fail_9_test, setup),
	cmocka_unit_test_setup(cfg_parse_master_test, setup),
	cmocka_unit_test_setup(cfg_parse_slave_test, setup),
	cmocka_unit_test_setup(cfg_parse_tor_test, setup),
	cmocka_unit_test_setup(cfg_parse_rxs_test, setup),
	};
	return cmocka_run_group_tests(tests, grp_setup, grp_teardown);
}

#ifdef __cplusplus
}
#endif

