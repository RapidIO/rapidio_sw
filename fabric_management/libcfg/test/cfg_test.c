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
#include <stdarg.h>
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

#include "cfg.h"
#include "cfg_private.h"
#include "libcli.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_SUCCESS (char *)("test/master_success.cfg")
#define SLAVE_SUCCESS (char *)("test/slave_success.cfg")

int test_case_1(void)
{
	char *dd_mtx_fn, *dd_fn;
	char *test_dd_mtx_fn = (char *)CFG_DFLT_DD_MTX_FN;
	char *test_dd_fn = (char *)CFG_DFLT_DD_FN;

	uint32_t m_did, m_cm_port, m_mode;

	if (cfg_parse_file(MASTER_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode))
		goto fail;

	if (strncmp(dd_mtx_fn, test_dd_mtx_fn, strlen(dd_mtx_fn)))
		goto fail;

	if (strncmp(dd_fn, test_dd_fn, strlen(test_dd_fn)))
		goto fail;

	if (5 != m_did)
		goto fail;

	if (CFG_DFLT_MAST_CM_PORT != m_cm_port)
		goto fail;

	if (!m_mode)
		goto fail;

	return 0;
fail:
	return 1;

};

int test_case_2(void)
{
	int i;
	int rc = 1;
	char fn[90];

	char *dd_mtx_fn = NULL, *dd_fn = NULL;
	uint32_t m_did, m_cm_port, m_mode;
	

	for (i = 0; (i < 6) && rc; i++) {
		snprintf(fn, 90, "test/parse_fail_%d.cfg", i);
	
		rc = cfg_parse_file(fn, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode);
		free(cfg);
		if (rc)
			printf("\nTest case 2 test %d passed", i);
		else
			printf("\nTest case 2 test %d FAILED", i);
	};

	return !rc;
};

int test_case_3(void)
{
	struct cfg_mport_info mp;
	struct cfg_dev dev;
	int conn_pt;
	char *dd_mtx_fn = NULL, *dd_fn = NULL;
	uint32_t m_did, m_cm_port, m_mode;

	if (cfg_parse_file(MASTER_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode))
		goto fail;

	if (cfg_find_mport(0, &mp))
		goto fail;

	if (mp.num != 0)
		goto fail;
	if (mp.ct != 0x10005)
		goto fail;
	if (mp.op_mode != 1)
		goto fail;

	if (!cfg_find_mport(1, &mp))
		goto fail;

	if (cfg_find_dev_by_ct(0x10005, &dev))
		goto fail;

	if (cfg_find_dev_by_ct(0x20006, &dev))
		goto fail;

	if (cfg_find_dev_by_ct(0x30007, &dev))
		goto fail;

	if (cfg_find_dev_by_ct(0x40008, &dev))
		goto fail;

	if (cfg_find_dev_by_ct(0x70000, &dev))
		goto fail;

	if (cfg_get_conn_dev(0x70000, 0, &dev, &conn_pt))
		goto fail;

	if (cfg_get_conn_dev(0x70000, 1, &dev, &conn_pt))
		goto fail;

	if (cfg_get_conn_dev(0x70000, 4, &dev, &conn_pt))
		goto fail;

	if (cfg_get_conn_dev(0x70000, 5, &dev, &conn_pt))
		goto fail;

	if (!cfg_get_conn_dev(0x70000, 7, &dev, &conn_pt))
		goto fail;

	if (cfg_get_conn_dev(0x10005, 0, &dev, &conn_pt))
		goto fail;

	if (cfg_get_conn_dev(0x20006, 0, &dev, &conn_pt))
		goto fail;

	if (cfg_get_conn_dev(0x30007, 0, &dev, &conn_pt))
		goto fail;

	if (cfg_get_conn_dev(0x40008, 0, &dev, &conn_pt))
		goto fail;

	if (!cfg_get_conn_dev(0x40008, 1, &dev, &conn_pt))
		goto fail;

	if (!cfg_find_dev_by_ct(0x99999, &dev))
		goto fail;

	return 0;
fail:
	return 1;
};
	
int test_case_4(void)
{
	struct cfg_mport_info mp;
	struct cfg_dev dev;
	char *dd_mtx_fn = NULL, *dd_fn = NULL;
	char *test_dd_mtx_fn = (char *)CFG_DFLT_DD_MTX_FN;
	char *test_dd_fn = (char *)CFG_DFLT_DD_FN;
	uint32_t m_did, m_cm_port, m_mode;

	if (cfg_parse_file(SLAVE_SUCCESS, &dd_mtx_fn, &dd_fn, &m_did,
			&m_cm_port, &m_mode))
		goto fail;

	if (strncmp(dd_mtx_fn, test_dd_mtx_fn, strlen(dd_mtx_fn)))
		goto fail;

	if (strncmp(dd_fn, test_dd_fn, strlen(test_dd_fn)))
		goto fail;

	if (5 != m_did)
		goto fail;

	if (CFG_DFLT_MAST_CM_PORT != m_cm_port)
		goto fail;

	if (m_mode)
		goto fail;

	if (cfg_find_mport(0, &mp))
		goto fail;

	if (!cfg_find_mport(3, &mp))
		goto fail;

	if (cfg_find_dev_by_ct(0x20006, &dev))
		goto fail;

	if (!cfg_find_dev_by_ct(0x70000, &dev))
		goto fail;

	return 0;
fail:
	return 1;
};
	
int main(int argc, char *argv[])
{
	int rc = EXIT_FAILURE;

	if (0)
		argv[0][0] = argc;

	rdma_log_init("cfg_test.log", 1);

	g_level = 1;

	if (test_case_1()) {
		printf("\nTest_case_1 FAILED.");
		goto fail;
	};
	printf("\nTest_case_1 passed.");
	free(cfg);

	if (test_case_2()) {
		printf("\nTest_case_2 FAILED.");
		goto fail;
	};
	printf("\nTest_case_2 passed.");

	if (test_case_3()) {
		printf("\nTest_case_3 FAILED.");
		goto fail;
	};
	printf("\nTest_case_3 passed.");
	free(cfg);

	if (test_case_4()) {
		printf("\nTest_case_4 FAILED.");
		goto fail;
	};
	printf("\nTest_case_4 passed.");
	free(cfg);

	rc = EXIT_SUCCESS;
fail:
	printf("\n");
	if (rc != EXIT_SUCCESS)
		rdma_log_dump();
	printf("\n");
	rdma_log_close();
	exit(rc);
};

#ifdef __cplusplus
}
#endif

