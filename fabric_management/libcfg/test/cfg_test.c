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
#define MASTER16_SUCCESS	(char *)("test/master16_success.cfg")
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
	rdma_log_init("cfg_test.log", 7);
	g_level = RDMA_LL_OFF;
	// g_disp_level = RDMA_LL_OFF;
	g_disp_level = 7;

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

static void cfg_parse_mport_dev16_size_test(void **state)
{
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	char *test_dd_mtx_fn = (char *)FMD_DFLT_DD_MTX_FN;
	char *test_dd_fn  = (char *)FMD_DFLT_DD_FN;
	uint8_t mem_sz;
	did_t m_did;
	uint32_t m_cm_port;
	uint32_t m_mode;

	g_disp_level = 7;
	check_file_exists(MASTER16_SUCCESS);

	m_did = DID_INVALID_ID;
	m_cm_port = 0xbabe;
	m_mode = 0xdead;
	assert_int_equal(0, cfg_parse_file(MASTER16_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode));
	assert_int_equal(0, strncmp(dd_mtx_fn, test_dd_mtx_fn, strlen(dd_mtx_fn)));
	assert_int_equal(0, strncmp(dd_fn, test_dd_fn, strlen(test_dd_fn)));
	assert_int_equal(0x5555, did_get_value(m_did));
	assert_int_equal(dev16_sz, did_get_size(m_did));
	assert_int_equal(FMD_DFLT_MAST_CM_PORT, m_cm_port);
	assert_int_equal(1, m_mode);

	mem_sz = 0xca;
	assert_int_equal(0, cfg_get_mp_mem_sz(0, &mem_sz));
	assert_int_equal(CFG_MEM_SZ_66, mem_sz);

	mem_sz = 0xfe;
	assert_int_equal(0, cfg_get_mp_mem_sz(1, &mem_sz));
	assert_int_equal(CFG_MEM_SZ_34, mem_sz);

	mem_sz = 0xba;
	assert_int_equal(0, cfg_get_mp_mem_sz(2, &mem_sz));
	assert_int_equal (CFG_MEM_SZ_50, mem_sz);

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

	count++; (void)state; // unused
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

static void cfg_assign_dev16_rt_test(void **state)
{
	did_val_t start_did, end_did;
	rio_rt_state_t rt;

	// Set all dids from 0x0000 to 0x0fff to point to port 12.
	// Must preserve Domain entry 0:
	// - all device entries are set to 12
	// - domain entry 0 remains RIO_RTE_LVL_G0
	// - domain entries 1 through 0xF are set to 12
	// - domain entries 0x10 through 0xF are set to 12
	start_did = 0x0000;
	end_did = 0x0FFF;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, 12, &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		assert_true(rt.dev_table[idx].changed);
		assert_int_equal(12, rt.dev_table[idx].rte_val);
		if (!idx || idx > 0xF) {
			assert_false(rt.dom_table[idx].changed);
		} else {
			assert_true(rt.dom_table[idx].changed);
		}
		if (idx > 0xF) {
			assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
		} else if (idx) {
			assert_int_equal(12, rt.dom_table[idx].rte_val);
		} else {
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
		}
	}

	// Set all dids from 0x0100 to 0x01ff to point to port 12.
	// - domain entry 1 is set to 12
	// - all other device/domain entries remain unchanged
	start_did = 0x0100;
	end_did = 0x01FF;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, 12, &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		assert_false(rt.dev_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dev_table[idx].rte_val);
		if (1 == idx) {
			assert_true(rt.dom_table[idx].changed);
			assert_int_equal(12, rt.dom_table[idx].rte_val);
		} else {
			assert_false(rt.dom_table[idx].changed);
			if (idx) {
				assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
			} else {
				assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			}
		}
	}

	// Set all dids from 0x0107 to 0x016C to point to port 12.
	// Must preserve Domain entry 0:
	// - domain entry 1 through 0xF is set to 12
	// - all other device/domain entries remain unchanged
	start_did = 0x0107;
	end_did = 0x016C;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, 12, &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		if (idx >= 7 && idx <= 0x6C) {
			assert_true(rt.dev_table[idx].changed);
			assert_int_equal(12, rt.dev_table[idx].rte_val);
		} else {
			assert_false(rt.dev_table[idx].changed);
			assert_int_equal(RIO_RTE_DROP, rt.dev_table[idx].rte_val);
		}
		if (!idx) {
			assert_false(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		};
		if (1 == idx) {
			assert_true(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		}
		assert_false(rt.dom_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
	}

	// Set all dids from 0x0107 to 0x016C to point to MC MASK 12.
	// Must route to MC mask in device table.
	// - domain entry 1 is set to RIO_RTE_LVL_G0
	// - device entries 0x07 through 0x6c are set to MC MASK 12
	// - all other device/domain entries remain unchanged
	start_did = 0x0107;
	end_did = 0x016C;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, RIO_RTV_MC_MSK(12), &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		if (idx >= 7 && idx <= 0x6C) {
			assert_true(rt.dev_table[idx].changed);
			assert_int_equal(RIO_RTV_MC_MSK(12), rt.dev_table[idx].rte_val);
		} else {
			assert_false(rt.dev_table[idx].changed);
			assert_int_equal(RIO_RTE_DROP, rt.dev_table[idx].rte_val);
		}
		if (!idx) {
			assert_false(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		};
		if (1 == idx) {
			assert_true(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		}
		assert_false(rt.dom_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
	}

	// Set all dids from 0x0200 to 0x02FF to point to MC MASK 12.
	// Must route to MC mask in device table.
	// - domain entry 1 is set to RIO_RTE_LVL_G0
	// - device entries 0x07 through 0x6c are set to MC MASK 12
	// - all other device/domain entries remain unchanged
	start_did = 0x0200;
	end_did = 0x02FF;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, RIO_RTV_MC_MSK(12), &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		assert_true(rt.dev_table[idx].changed);
		assert_int_equal(RIO_RTV_MC_MSK(12), rt.dev_table[idx].rte_val);
		if (!idx) {
			assert_false(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		};
		if (2 == idx) {
			assert_true(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		}
		assert_false(rt.dom_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
	}

	// Set did 0x0007 to 0x0439 to point to port 12.
	start_did = 0x0007;
	end_did = 0x0007;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, 12, &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		assert_false(rt.dom_table[idx].changed);
		if (!idx) {
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
		} else {
			assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
		}
		if (7 == idx) {
			assert_true(rt.dev_table[idx].changed);
			assert_int_equal(12, rt.dev_table[idx].rte_val);
			continue;
		}
		assert_false(rt.dev_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dev_table[idx].rte_val);
	}

	// Set all dids from 0x0041 to 0x0049 to drop packets.
	// This should not result in any changes in the routing table.
	start_did = 0x0041;
	end_did = 0x0049;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, RIO_RTE_DROP, &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		assert_false(rt.dev_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dev_table[idx].rte_val);
		if (rt.dom_table[idx].changed) {
			assert_int_equal(idx, 0xFFFFFFFF);
		}
		assert_false(rt.dom_table[idx].changed);
		if (!idx) {
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		};
		assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
	}

	// Set all dids from 0x0041 to 0x0049 to use the default port packets.
	// This should result in changes to the device routing table.
	start_did = 0x0041;
	end_did = 0x0049;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, RIO_RTE_DFLT_PORT, &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		if (idx >= start_did && idx <= end_did){
			assert_true(rt.dev_table[idx].changed);
			assert_int_equal(RIO_RTE_DFLT_PORT, rt.dev_table[idx].rte_val);
		} else {
			assert_false(rt.dev_table[idx].changed);
			assert_int_equal(RIO_RTE_DROP, rt.dev_table[idx].rte_val);
		}
		if (rt.dom_table[idx].changed) {
			assert_int_equal(idx, 0xFFFFFFFF);
		}
		assert_false(rt.dom_table[idx].changed);
		if (!idx) {
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		};
		assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
	}

	// Set did 0x69E0 to point to drop packets.
	// This should change the domain table, but not the dev table.
	start_did = 0x69e0;
	end_did = 0x69e0;
	init_rt(&rt);
	assert_int_equal(0, assign_dev16_rt_v(start_did, end_did, RIO_RTE_DROP, &rt, cfg));
	for (uint32_t idx = 0; idx < RIO_RT_GRP_SZ; idx++) {
		assert_false(rt.dev_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dev_table[idx].rte_val);
		if (!idx) {
			assert_false(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		}
		if (0x69 == idx) {
			assert_true(rt.dom_table[idx].changed);
			assert_int_equal(RIO_RTE_LVL_G0, rt.dom_table[idx].rte_val);
			continue;
		};
		assert_false(rt.dom_table[idx].changed);
		assert_int_equal(RIO_RTE_DROP, rt.dom_table[idx].rte_val);
	}


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
	rio_rt_state_t *rt;

	check_file_exists(MASTER_SUCCESS);
	memset(&dev, 0, sizeof(dev));

	assert_int_equal(0, cfg_parse_file(MASTER_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode));

	assert_int_equal(0, cfg_find_mport(0, &mp));
	assert_int_equal(0, mp.num);
	assert_int_equal(0x10005, mp.ct);
	assert_int_equal(5, mp.devids[CFG_DEV08].did_val);
	assert_int_equal(0xFF, mp.devids[CFG_DEV08].hc);
	assert_true(mp.devids[CFG_DEV08].valid);
	assert_int_equal(0x5555, mp.devids[CFG_DEV16].did_val);
	assert_int_equal(0xFF, mp.devids[CFG_DEV16].hc);
	assert_true(mp.devids[CFG_DEV16].valid);
	assert_false(mp.devids[CFG_DEV32].valid);
	assert_int_not_equal(0, cfg_find_mport(3, &mp));

	assert_int_equal(0, cfg_find_dev_by_ct(0x10005, &dev));
	assert_string_equal("GRYPHON_01", dev.name);
	assert_int_equal(0x10005, dev.ct);
	assert_int_equal(0, dev.is_sw);
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_5p0, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_two, dev.ep_pt.iseq);
	assert_true(dev.ep_pt.devids[CFG_DEV08].valid);
	assert_int_equal(5, dev.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(0xFF, dev.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x5555, dev.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(0xFF, dev.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x20006, &dev));
	assert_int_equal(0x20006, dev.ct);
	assert_string_equal("GRYPHON_02", dev.name);
	assert_int_equal(0, dev.is_sw);
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_6p25, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_two, dev.ep_pt.iseq);
	assert_true(dev.ep_pt.devids[CFG_DEV08].valid);
	assert_int_equal(6, dev.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(1, dev.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x6666, dev.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(1, dev.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x30007, &dev));
	assert_string_equal("GRYPHON_03", dev.name);
	assert_int_equal(0, dev.is_sw);
	assert_int_equal(0x30007, dev.ct);
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_1p25, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_one, dev.ep_pt.iseq);
	assert_true(dev.ep_pt.devids[CFG_DEV08].valid);
	assert_int_equal(7, dev.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(1, dev.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x7777, dev.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(1, dev.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x40008, &dev));
	assert_string_equal("GRYPHON_04", dev.name);
	assert_int_equal(0x40008, dev.ct);
	assert_int_equal(0, dev.is_sw);
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_2p5, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_one, dev.ep_pt.iseq);
	assert_int_equal(8, dev.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(1, dev.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x8888, dev.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(1, dev.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x70009, &dev));
	assert_string_equal("MAIN_SWITCH", dev.name);
	assert_int_equal(0x70009, dev.ct);
	assert_int_equal(1, dev.is_sw);
	assert_int_equal(24, dev.sw_info.num_ports);
	for (rio_port_t pt = 0; pt < dev.sw_info.num_ports; pt++) {
		// Per port routing tables are all invalid.
		for (uint32_t idx = 0; idx < CFG_DEVID_MAX; idx++) {
			assert_null(dev.sw_info.sw_pt[pt].rt[idx]);
		}
		// Undefined ports
		if (pt >= 18) {
			assert_false(dev.sw_info.sw_pt[pt].valid);
			assert_int_equal(rio_pc_pw_last, dev.sw_info.sw_pt[pt].op_pw);
			assert_int_equal(rio_pc_is_last, dev.sw_info.sw_pt[pt].iseq);
			continue;
		}
		assert_int_equal(pt, dev.sw_info.sw_pt[pt].port);
		assert_int_equal(1, dev.sw_info.sw_pt[pt].valid);
		switch (pt) {
		case 1:
		case 8:
		case 11:
			assert_int_equal(rio_pc_pw_2x, dev.sw_info.sw_pt[pt].op_pw);
			break;
		case 2:
		case 3:
		case 9:
		case 10:
			assert_int_equal(rio_pc_pw_1x, dev.sw_info.sw_pt[pt].op_pw);
			break;
		default:
			assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[pt].op_pw);
		}
		switch (pt) {
		case 0:
			assert_int_equal(rio_pc_ls_6p25, dev.sw_info.sw_pt[0].ls);
			break;
		case 2:
			assert_int_equal(rio_pc_ls_3p125, dev.sw_info.sw_pt[2].ls);
			break;
		case 3:
			assert_int_equal(rio_pc_ls_2p5, dev.sw_info.sw_pt[3].ls);
			break;
		case 4:
			assert_int_equal(rio_pc_ls_1p25, dev.sw_info.sw_pt[4].ls);
			break;
		default:
			assert_int_equal(rio_pc_ls_5p0, dev.sw_info.sw_pt[pt].ls);
		}
		if (pt < 4) {
			assert_int_equal(rio_pc_is_two, dev.sw_info.sw_pt[pt].iseq);
		} else {
			assert_int_equal(rio_pc_is_one, dev.sw_info.sw_pt[pt].iseq);
		}
	}
	// Check all 256 entries of dev08 global routing domain
	// and multicast group tables.
	assert_non_null(dev.sw_info.rt[CFG_DEV08]);
	rt = dev.sw_info.rt[CFG_DEV08];
	assert_int_equal(RT_VAL_DROP, rt->default_route);
	for (uint16_t rti = 0; rti < RIO_RT_GRP_SZ; rti++) {
		assert_false(rt->dom_table[rti].changed);
		if (rti) {
			assert_int_equal(RT_VAL_DROP, rt->dom_table[rti].rte_val);
		} else {
			assert_int_equal(RIO_RTE_LVL_G0, rt->dom_table[rti].rte_val);
		}
		if (5 == rti) {
			assert_int_equal(1, rt->mc_masks[rti].in_use);
			assert_int_equal(1, rt->mc_masks[rti].allocd);
			assert_int_equal(1, rt->mc_masks[rti].changed);
			assert_int_equal(tt_dev8, rt->mc_masks[rti].tt);
			assert_int_equal(0x3FFFF, rt->mc_masks[rti].mc_mask);
			assert_int_equal(0x51, rt->mc_masks[rti].mc_destID);
		} else {
			assert_int_equal(0, rt->mc_masks[rti].in_use);
		}
	}
	// Check all 256 entries of dev08 global routing device table
	assert_int_equal(RT_VAL_DROP, rt->dev_table[0].rte_val);
	assert_int_equal(0, rt->dev_table[5].rte_val);
	assert_int_equal(4, rt->dev_table[6].rte_val);
	assert_int_equal(1, rt->dev_table[7].rte_val);
	assert_int_equal(5, rt->dev_table[8].rte_val);
	assert_int_equal(RT_VAL_DEFAULT_ROUTE, rt->dev_table[0x20].rte_val);
	assert_int_equal(13, rt->dev_table[0x30].rte_val);
	assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[0x40].rte_val);
	assert_int_equal(14, rt->dev_table[0x50].rte_val);
	assert_int_equal(RT_VAL_DROP, rt->dev_table[0x60].rte_val);
	for (uint16_t i = 0x21; i <= 0x29; i++) {
		assert_int_equal(13, rt->dev_table[i].rte_val);
		assert_int_equal(RT_VAL_DEFAULT_ROUTE, rt->dev_table[i+0x10].rte_val);
		assert_int_equal(RT_VAL_DROP, rt->dev_table[i+0x20].rte_val);
		assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[i+0x30].rte_val);
		assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[i+0x40].rte_val);
		assert_int_equal(RT_VAL_DROP, rt->dev_table[i+0x50].rte_val);
	}

	for (uint16_t rti = 0; rti < RIO_RT_GRP_SZ; rti++) {
		uint8_t up = (rti & 0xF0) >> 4;
		uint8_t lo = rti & 0x0F;
		if (rti >= 5 && rti <= 8) {
			assert_true(rt->dev_table[rti].changed);
			continue;
		}
		if (rti == 0x40) {
			assert_true(rt->dev_table[rti].changed);
			continue;
		}
		if (rti == 0x60 || rti == 0x70) {
			assert_false(rt->dev_table[rti].changed);
			continue;
		}
		if ((2 == up || 3 == up || 5 == up || 6 == up) && lo <= 9) {
			if (!rt->dev_table[rti].changed) {
				assert_int_equal(rti, rt->dev_table[rti].rte_val);
			}
			assert_true(rt->dev_table[rti].changed);
			continue;
		}
		assert_int_equal(RT_VAL_DROP, rt->dev_table[rti].rte_val);
		assert_false(rt->dev_table[rti].changed);
	}

	// Check connections were set up correctly
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 0, &dev, &conn_pt));
	assert_string_equal("GRYPHON_01", dev.name);
	assert_int_equal(0x10005, dev.ct);
	assert_false(dev.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 1, &dev, &conn_pt));
	assert_string_equal("GRYPHON_03", dev.name);
	assert_int_equal(0x30007, dev.ct);
	assert_false(dev.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 4, &dev, &conn_pt));
	assert_string_equal("GRYPHON_02", dev.name);
	assert_int_equal(0x20006, dev.ct);
	assert_false(dev.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 5, &dev, &conn_pt));
	assert_string_equal("GRYPHON_04", dev.name);
	assert_int_equal(0x40008, dev.ct);
	assert_false(dev.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_not_equal(0, cfg_get_conn_dev(0x70009, 7, &dev, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x10005, 0, &dev, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev.name);
	assert_int_equal(0x70009, dev.ct);
	assert_true(dev.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x20006, 0, &dev, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev.name);
	assert_int_equal(0x70009, dev.ct);
	assert_true(dev.is_sw);
	assert_int_equal(4, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x30007, 0, &dev, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev.name);
	assert_int_equal(0x70009, dev.ct);
	assert_true(dev.is_sw);
	assert_int_equal(1, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x40008, 0, &dev, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev.name);
	assert_int_equal(0x70009, dev.ct);
	assert_true(dev.is_sw);
	assert_int_equal(5, conn_pt);
	assert_int_not_equal(0, cfg_get_conn_dev(0x40008, 1, &dev, &conn_pt));
	assert_int_not_equal(0,cfg_find_dev_by_ct(0x99999, &dev));

	count++;
	free(cfg);
	(void)state; // unused
}

static void cfg_parse_master16_test(void **state)
{
	struct cfg_mport_info mp;
	struct cfg_dev dev16;
	int conn_pt;
	char *dd_mtx_fn = NULL;
	char *dd_fn = NULL;
	did_t m_did = DID_INVALID_ID;
	uint32_t m_cm_port = 0xbeef;
	uint32_t m_mode = 0xcafe;
	rio_rt_state_t *rt;

	check_file_exists(MASTER16_SUCCESS);
	memset(&dev16, 0, sizeof(dev16));

	assert_int_equal(0, cfg_parse_file(MASTER16_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode));

	assert_int_equal(0, cfg_find_mport(0, &mp));
	assert_int_equal(0, mp.num);
	assert_int_equal(0x10005, mp.ct);
	assert_int_equal(5, mp.devids[CFG_DEV08].did_val);
	assert_int_equal(0xFF, mp.devids[CFG_DEV08].hc);
	assert_true(mp.devids[CFG_DEV08].valid);
	assert_int_equal(0x5555, mp.devids[CFG_DEV16].did_val);
	assert_int_equal(0xFF, mp.devids[CFG_DEV16].hc);
	assert_true(mp.devids[CFG_DEV16].valid);
	assert_false(mp.devids[CFG_DEV32].valid);
	assert_int_not_equal(0, cfg_find_mport(3, &mp));

	assert_int_equal(0, cfg_find_dev_by_ct(0x10005, &dev16));
	assert_string_equal("GRYPHON_01", dev16.name);
	assert_int_equal(0x10005, dev16.ct);
	assert_int_equal(0, dev16.is_sw);
	assert_int_equal(rio_pc_pw_4x, dev16.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_5p0, dev16.ep_pt.ls);
	assert_int_equal(rio_pc_is_two, dev16.ep_pt.iseq);
	assert_true(dev16.ep_pt.devids[CFG_DEV08].valid);
	assert_int_equal(5, dev16.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(0xFF, dev16.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev16.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x5555, dev16.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(0xFF, dev16.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x20006, &dev16));
	assert_int_equal(0x20006, dev16.ct);
	assert_string_equal("GRYPHON_02", dev16.name);
	assert_int_equal(0, dev16.is_sw);
	assert_int_equal(rio_pc_pw_2x, dev16.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_6p25, dev16.ep_pt.ls);
	assert_int_equal(rio_pc_is_two, dev16.ep_pt.iseq);
	assert_true(dev16.ep_pt.devids[CFG_DEV08].valid);
	assert_int_equal(6, dev16.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(1, dev16.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev16.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x6666, dev16.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(1, dev16.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x30007, &dev16));
	assert_string_equal("GRYPHON_03", dev16.name);
	assert_int_equal(0, dev16.is_sw);
	assert_int_equal(0x30007, dev16.ct);
	assert_int_equal(rio_pc_pw_2x, dev16.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_1p25, dev16.ep_pt.ls);
	assert_int_equal(rio_pc_is_one, dev16.ep_pt.iseq);
	assert_true(dev16.ep_pt.devids[CFG_DEV08].valid);
	assert_int_equal(7, dev16.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(1, dev16.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev16.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x7777, dev16.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(1, dev16.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x40008, &dev16));
	assert_string_equal("GRYPHON_04", dev16.name);
	assert_int_equal(0x40008, dev16.ct);
	assert_int_equal(0, dev16.is_sw);
	assert_int_equal(rio_pc_pw_4x, dev16.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_2p5, dev16.ep_pt.ls);
	assert_int_equal(rio_pc_is_one, dev16.ep_pt.iseq);
	assert_int_equal(8, dev16.ep_pt.devids[CFG_DEV08].did_val);
	assert_int_equal(1, dev16.ep_pt.devids[CFG_DEV08].hc);
	assert_true(dev16.ep_pt.devids[CFG_DEV16].valid);
	assert_int_equal(0x8888, dev16.ep_pt.devids[CFG_DEV16].did_val);
	assert_int_equal(1, dev16.ep_pt.devids[CFG_DEV16].hc);

	assert_int_equal(0, cfg_find_dev_by_ct(0x70009, &dev16));
	assert_string_equal("MAIN_SWITCH", dev16.name);
	assert_int_equal(0x70009, dev16.ct);
	assert_int_equal(1, dev16.is_sw);
	assert_int_equal(24, dev16.sw_info.num_ports);
	for (rio_port_t pt = 0; pt < dev16.sw_info.num_ports; pt++) {
		// Per port routing tables are all invalid.
		for (uint32_t idx = 0; idx < CFG_DEVID_MAX; idx++) {
			assert_null(dev16.sw_info.sw_pt[pt].rt[idx]);
		}
		// Undefined ports
		if (pt >= 18) {
			assert_false(dev16.sw_info.sw_pt[pt].valid);
			assert_int_equal(rio_pc_pw_last, dev16.sw_info.sw_pt[pt].op_pw);
			assert_int_equal(rio_pc_is_last, dev16.sw_info.sw_pt[pt].iseq);
			continue;
		}
		assert_int_equal(pt, dev16.sw_info.sw_pt[pt].port);
		assert_int_equal(1, dev16.sw_info.sw_pt[pt].valid);
		switch (pt) {
		case 1:
		case 8:
		case 11:
			assert_int_equal(rio_pc_pw_2x, dev16.sw_info.sw_pt[pt].op_pw);
			break;
		case 2:
		case 3:
		case 9:
		case 10:
			assert_int_equal(rio_pc_pw_1x, dev16.sw_info.sw_pt[pt].op_pw);
			break;
		default:
			assert_int_equal(rio_pc_pw_4x, dev16.sw_info.sw_pt[pt].op_pw);
		}
		switch (pt) {
		case 0:
			assert_int_equal(rio_pc_ls_6p25, dev16.sw_info.sw_pt[0].ls);
			break;
		case 2:
			assert_int_equal(rio_pc_ls_3p125, dev16.sw_info.sw_pt[2].ls);
			break;
		case 3:
			assert_int_equal(rio_pc_ls_2p5, dev16.sw_info.sw_pt[3].ls);
			break;
		case 4:
			assert_int_equal(rio_pc_ls_1p25, dev16.sw_info.sw_pt[4].ls);
			break;
		default:
			assert_int_equal(rio_pc_ls_5p0, dev16.sw_info.sw_pt[pt].ls);
		}
		if (pt < 4) {
			assert_int_equal(rio_pc_is_two, dev16.sw_info.sw_pt[pt].iseq);
		} else {
			assert_int_equal(rio_pc_is_one, dev16.sw_info.sw_pt[pt].iseq);
		}
	}
	assert_null(dev16.sw_info.rt[CFG_DEV08]);
	assert_non_null(dev16.sw_info.rt[CFG_DEV16]);
	rt = dev16.sw_info.rt[CFG_DEV16];
	assert_int_equal(RT_VAL_DROP, rt->default_route);
	// Check all 256 entries of the dev16 multicast group tables.
	for (uint16_t rti = 0; rti < RIO_RT_GRP_SZ; rti++) {
		if (5 == rti) {
			assert_int_equal(1, rt->mc_masks[rti].in_use);
			assert_int_equal(1, rt->mc_masks[rti].allocd);
			assert_int_equal(1, rt->mc_masks[rti].changed);
			assert_int_equal(tt_dev8, rt->mc_masks[rti].tt);
			assert_int_equal(0x3FFFF, rt->mc_masks[rti].mc_mask);
			assert_int_equal(0x81, rt->mc_masks[rti].mc_destID);
		} else {
			assert_int_equal(0, rt->mc_masks[rti].in_use);
		}
	}
	// Check all 256 entries of dev16 global routing domain table.
	for (uint16_t rti = 0; rti < RIO_RT_GRP_SZ; rti++) {
		switch (rti) {
		case 0: assert_int_equal(rt->dom_table[rti].rte_val,
		                        RIO_RTE_LVL_G0);
				assert_false(rt->dom_table[rti].changed);
				break;
		case 1: assert_int_equal(rt->dom_table[rti].rte_val,
		                        RIO_RTE_DFLT_PORT);
				assert_true(rt->dom_table[rti].changed);
				break;
		case 2: assert_int_equal(rt->dom_table[rti].rte_val, 12);
				assert_true(rt->dom_table[rti].changed);
				break;
		case 3:
		case 4:
		case 5: assert_int_equal(rt->dom_table[rti].rte_val, 9);
				assert_true(rt->dom_table[rti].changed);
				break;
		case 6:
		case 0x55: // GRYPHON_01
		case 0x66: // GRYPHON_02
		case 0x77: // GRYPHON_03
		case 0x88: // GRYPHON_04
		case 0x69: assert_int_equal(rt->dom_table[rti].rte_val,
		                        RIO_RTE_LVL_G0);
				assert_true(rt->dom_table[rti].changed);
				break;
		default:
				if (RIO_RTE_DROP != rt->dom_table[rti].rte_val) {
					assert_int_equal(rti, RIO_RTE_DROP);
				}
				assert_int_equal(rt->dom_table[rti].rte_val,
		                        RIO_RTE_DROP);
				assert_false(rt->dom_table[rti].changed);
				break;
		}
	}
	// Check all 256 entries of dev08 global routing device table
	assert_int_equal(RT_VAL_DROP, rt->dev_table[0].rte_val);
	assert_int_equal(12, rt->dev_table[0x07].rte_val);
	assert_int_equal(RIO_RTE_DFLT_PORT, rt->dev_table[0x20].rte_val);
	assert_int_equal(13, rt->dev_table[0x30].rte_val);
	assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[0x40].rte_val);
	assert_int_equal(14, rt->dev_table[0x50].rte_val);
	assert_int_equal(RIO_RTE_DROP, rt->dev_table[0x60].rte_val);
	assert_int_equal(0, rt->dev_table[0x55].rte_val);
	assert_int_equal(4, rt->dev_table[0x66].rte_val);
	assert_int_equal(1, rt->dev_table[0x77].rte_val);
	assert_int_equal(5, rt->dev_table[0x88].rte_val);
	assert_int_equal(RT_VAL_DEFAULT_ROUTE, rt->dev_table[0x20].rte_val);
	assert_int_equal(13, rt->dev_table[0x30].rte_val);
	assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[0x40].rte_val);
	assert_int_equal(14, rt->dev_table[0x50].rte_val);
	assert_int_equal(RT_VAL_DROP, rt->dev_table[0x60].rte_val);
	for (uint16_t i = 0x21; i <= 0x24; i++) {
		assert_int_equal(13, rt->dev_table[i].rte_val);
		assert_int_equal(RT_VAL_DEFAULT_ROUTE, rt->dev_table[i+0x10].rte_val);
		assert_int_equal(RT_VAL_DROP, rt->dev_table[i+0x20].rte_val);
		assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[i+0x60].rte_val);
		assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[i+0x70].rte_val);
		assert_int_equal(RT_VAL_DROP, rt->dev_table[i+0x80].rte_val);
		assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[i+0x90].rte_val);
	}
	for (uint16_t i = 0xC1; i <= 0xCF; i++) {
		assert_int_equal(10, rt->dev_table[i].rte_val);
	}
	assert_int_equal(12, rt->dev_table[0x97].rte_val);
	assert_int_equal(RT_VAL_DEFAULT_ROUTE, rt->dev_table[0xA0].rte_val);
	assert_int_equal(13, rt->dev_table[0xB0].rte_val);
	assert_int_equal(RIO_RTV_MC_MSK(5), rt->dev_table[0xC0].rte_val);
	assert_int_equal(14, rt->dev_table[0xD0].rte_val);
	assert_int_equal(RT_VAL_DROP, rt->dev_table[0xE0].rte_val);

	for (uint16_t rti = 0; rti < RIO_RT_GRP_SZ; rti++) {
		if (RT_VAL_DROP == rt->dev_table[rti].rte_val) {
			assert_false(rt->dev_table[rti].changed);
		} else {
			assert_true(rt->dev_table[rti].changed);
		}
	}

	// Check connections were set up correctly
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 0, &dev16, &conn_pt));
	assert_string_equal("GRYPHON_01", dev16.name);
	assert_int_equal(0x10005, dev16.ct);
	assert_false(dev16.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 1, &dev16, &conn_pt));
	assert_string_equal("GRYPHON_03", dev16.name);
	assert_int_equal(0x30007, dev16.ct);
	assert_false(dev16.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 4, &dev16, &conn_pt));
	assert_string_equal("GRYPHON_02", dev16.name);
	assert_int_equal(0x20006, dev16.ct);
	assert_false(dev16.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x70009, 5, &dev16, &conn_pt));
	assert_string_equal("GRYPHON_04", dev16.name);
	assert_int_equal(0x40008, dev16.ct);
	assert_false(dev16.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_not_equal(0, cfg_get_conn_dev(0x70009, 7, &dev16, &conn_pt));
	assert_int_equal(0, cfg_get_conn_dev(0x10005, 0, &dev16, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev16.name);
	assert_int_equal(0x70009, dev16.ct);
	assert_true(dev16.is_sw);
	assert_int_equal(0, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x20006, 0, &dev16, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev16.name);
	assert_int_equal(0x70009, dev16.ct);
	assert_true(dev16.is_sw);
	assert_int_equal(4, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x30007, 0, &dev16, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev16.name);
	assert_int_equal(0x70009, dev16.ct);
	assert_true(dev16.is_sw);
	assert_int_equal(1, conn_pt);
	assert_int_equal(0, cfg_get_conn_dev(0x40008, 0, &dev16, &conn_pt));
	assert_string_equal("MAIN_SWITCH", dev16.name);
	assert_int_equal(0x70009, dev16.ct);
	assert_true(dev16.is_sw);
	assert_int_equal(5, conn_pt);
	assert_int_not_equal(0, cfg_get_conn_dev(0x40008, 1, &dev16, &conn_pt));
	assert_int_not_equal(0,cfg_find_dev_by_ct(0x99999, &dev16));

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
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_5p0, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_two, dev.ep_pt.iseq);

	assert_int_equal(0, cfg_find_dev_by_ct(0x20006, &dev));
	assert_int_equal(rio_pc_pw_2x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_6p25, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_two, dev.ep_pt.iseq);

	assert_int_equal(0, cfg_find_dev_by_ct(0x30007, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_3p125, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_one, dev.ep_pt.iseq);

	assert_int_equal(0, cfg_find_dev_by_ct(0x40008, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_2p5, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_one, dev.ep_pt.iseq);

	assert_int_equal(0, cfg_find_dev_by_ct(0x50009, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_10p3, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_three, dev.ep_pt.iseq);

	assert_int_equal(0, cfg_find_dev_by_ct(0x6000A, &dev));
	assert_int_equal(rio_pc_pw_4x, dev.ep_pt.op_pw);
	assert_int_equal(rio_pc_ls_12p5, dev.ep_pt.ls);
	assert_int_equal(rio_pc_is_dflt, dev.ep_pt.iseq);

	assert_int_equal(0, cfg_find_dev_by_ct(0x7007b, &dev));

	assert_int_equal(1, dev.sw_info.sw_pt[1].valid);
	assert_int_equal(1, dev.sw_info.sw_pt[1].port);
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[1].op_pw);
	assert_int_equal(rio_pc_ls_5p0, dev.sw_info.sw_pt[1].ls);
	assert_int_equal(rio_pc_is_two, dev.sw_info.sw_pt[1].iseq);

	assert_int_equal(1, dev.sw_info.sw_pt[2].valid);
	assert_int_equal(2, dev.sw_info.sw_pt[2].port);
	assert_int_equal(rio_pc_pw_2x, dev.sw_info.sw_pt[2].op_pw);
	assert_int_equal(rio_pc_ls_6p25, dev.sw_info.sw_pt[2].ls);
	assert_int_equal(rio_pc_is_two, dev.sw_info.sw_pt[2].iseq);

	assert_int_equal(1, dev.sw_info.sw_pt[3].valid);
	assert_int_equal(3, dev.sw_info.sw_pt[3].port);
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[3].op_pw);
	assert_int_equal(rio_pc_ls_3p125, dev.sw_info.sw_pt[3].ls);
	assert_int_equal(rio_pc_is_one, dev.sw_info.sw_pt[3].iseq);

	assert_int_equal(1, dev.sw_info.sw_pt[4].valid);
	assert_int_equal(4, dev.sw_info.sw_pt[4].port);
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[4].op_pw);
	assert_int_equal(rio_pc_ls_2p5, dev.sw_info.sw_pt[4].ls);
	assert_int_equal(rio_pc_is_one, dev.sw_info.sw_pt[4].iseq);

	assert_int_equal(1, dev.sw_info.sw_pt[5].valid);
	assert_int_equal(5, dev.sw_info.sw_pt[5].port);
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[5].op_pw);
	assert_int_equal(rio_pc_ls_10p3, dev.sw_info.sw_pt[5].ls);
	assert_int_equal(rio_pc_is_three, dev.sw_info.sw_pt[5].iseq);

	assert_int_equal(1, dev.sw_info.sw_pt[6].valid);
	assert_int_equal(6, dev.sw_info.sw_pt[6].port);
	assert_int_equal(rio_pc_pw_4x, dev.sw_info.sw_pt[6].op_pw);
	assert_int_equal(rio_pc_ls_12p5, dev.sw_info.sw_pt[6].ls);
	assert_int_equal(rio_pc_is_dflt, dev.sw_info.sw_pt[6].iseq);

	assert_int_equal(0,cfg_get_conn_dev(0x7007b, 0, &dev, &conn_pt));

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
	cmocka_unit_test_setup(cfg_parse_mport_dev16_size_test, setup),
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
	cmocka_unit_test_setup(cfg_assign_dev16_rt_test, setup),
	cmocka_unit_test_setup(cfg_parse_master_test, setup),
	cmocka_unit_test_setup(cfg_parse_master16_test, setup),
	cmocka_unit_test_setup(cfg_parse_slave_test, setup),
	cmocka_unit_test_setup(cfg_parse_tor_test, setup),
	cmocka_unit_test_setup(cfg_parse_rxs_test, setup),
	};
	return cmocka_run_group_tests(tests, grp_setup, grp_teardown);
}

#ifdef __cplusplus
}
#endif

