/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
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

#include <stdint.h>
#include <stdio.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <math.h>
#include <time.h>

#include "rio_misc.h"
#include "rio_route.h"
#include "tok_parse.h"
#include "rapidio_mport_dma.h"
#include "string_util.h"
#include "goodput_cli.h"
#include "libtime_utils.h"
#include "librsvdmem.h"
#include "liblog.h"
#include "assert.h"
#include "math_util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLOAT_STR_SIZE 20

char *req_type_str[(int)last_action+1] = {
	(char *)"NO_ACT",
	(char *)"DIO",
	(char *)"ioTlat",
	(char *)"ioRlat",
	(char *)"DMA",
	(char *)"DmaNum",
	(char *)"dT_Lat",
	(char *)"dR_Lat",
	(char *)"MSG_Tx",
	(char *)"mT_Lat",
	(char *)"mTx_Oh",
	(char *)"MSG_Rx",
	(char *)"mR_Lat",
	(char *)"mRx_Oh",
	(char *)" IBWIN",
	(char *)"~IBWIN",
	(char *)"SHTDWN",
	(char *)"LAST"
};

// Parse the token ensuring it is within the range for a worker index and
// check the status of the worker thread.
static int gp_parse_worker_index(struct cli_env *env, char *tok, uint16_t *idx)
{
	if (tok_parse_ushort(tok, idx, 0, MAX_WORKER_IDX, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<idx>", 0, MAX_WORKER_IDX);
		return 1;
	}
	return 0;
}

// Parse the token ensuring it is within the range for a worker index and
// check the status of the worker thread.
static int gp_parse_worker_index_check_thread(struct cli_env *env, char *tok,
		uint16_t *idx, bool want_halted)
{
	if (gp_parse_worker_index(env, tok, idx)) {
		goto err;
	}

	if (want_halted) {
		if (2 != wkr[*idx].stat) {
			LOGMSG(env, "\nWorker not halted\n");
			goto err;
		}
	} else {
		if (2 == wkr[*idx].stat) {
			LOGMSG(env, "\nWorker halted\n");
			goto err;
		}
	}
	return 0;
err:
	return 1;
}

// Parse the token as a boolean value. The range of the token is restricted
// to the numeric values of 0 (false) and 1 (true)
static int gp_parse_bool(struct cli_env *env, char *tok, const char *name, uint16_t *boo)
{
	if (tok_parse_ushort(tok, boo, 0, 1, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, name, 0, 1);
		return 1;
	}
	return 0;
}

// Parse the token ensuring it is within the provided range. Further ensure it
// is a power of 2
static int gp_parse_ull_pw2(struct cli_env *env, char *tok, const char *name,
		uint64_t *value, uint64_t min, uint64_t max)
{
	if (tok_parse_ulonglong(tok, value, min, max, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, name, min, max);
		goto err;
	}

	if ((*value - 1) & *value) {
		LOGMSG(env, "\n%s must be a power of 2\n", name);
		goto err;
	}

	return 0;
err:
	return 1;
}

static int gp_parse_cpu(struct cli_env *env, char *dec_parm, int *cpu)
{
	const int MAX_GOODPUT_CPU = getCPUCount() - 1;

	if (tok_parse_long(dec_parm, cpu, -1, MAX_GOODPUT_CPU, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_LONG_MSG_FMT, "<cpu>", -1, MAX_GOODPUT_CPU);
		return 1;
	}
	return 0;
}

static int gp_parse_did(struct cli_env *env, char *tok, did_val_t *did_val)
{
	if (tok_parse_did(tok, did_val, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, "<did> must be between 0 and 0xff\n");
		return 1;
	}
	return 0;
}

#define ACTION_STR(x) (char *)((x < last_action)?req_type_str[x]:"UNKWN!")
#define MODE_STR(x) (char *)((x == kernel_action)?"KRNL":"User")
#define THREAD_STR(x) (char *)((0 == x)?"---":((1 == x)?"Run":"Hlt"))

static int ThreadCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;
	uint16_t new_dma;
	int cpu;

	if (gp_parse_worker_index(env, argv[0], &idx)) {
		goto exit;
	}

	if (gp_parse_cpu(env, argv[1], &cpu)) {
		goto exit;
	}

	if (gp_parse_bool(env, argv[2], "<new_dma>", &new_dma)) {
		goto exit;
	}

	if (wkr[idx].stat) {
		LOGMSG(env, "\nWorker %u already alive\n", idx);
		goto exit;
	}

	wkr[idx].idx = (int)idx;
	start_worker_thread(&wkr[idx], (int)new_dma, cpu);

exit:
	return 0;
}

struct cli_cmd Thread = {
"thread",
1,
3,
"Start a thread on a cpu",
"start <idx> <cpu> <new_dma>\n"
	"<idx>     is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<cpu>     is a cpu number, or -1 to indicate no cpu affinity\n"
	"<new_dma> 0: share DMA channel, 1: try to get a new DMA channel\n",
ThreadCmd,
ATTR_NONE
};

static int KillCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t st_idx = 0, end_idx = MAX_WORKER_IDX, i;

	if (strncmp(argv[0], "all", 3)) {
		if (gp_parse_worker_index(env, argv[0], &st_idx)) {
			goto exit;
		}
		end_idx = st_idx;
	}

	for (i = st_idx; i <= end_idx; i++) {
		shutdown_worker_thread(&wkr[i]);
	}

exit:
	return 0;
}

struct cli_cmd Kill = {
"kill",
4,
1,
"Kill a thread",
"kill <idx>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) ", or \"all\"\n",
KillCmd,
ATTR_NONE
};

static int HaltCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t st_idx = 0, end_idx = MAX_WORKER_IDX, i;

	if (strncmp(argv[0], "all", 3)) {
		if (gp_parse_worker_index(env, argv[0], &st_idx)) {
			goto exit;
		}
		end_idx = st_idx;
	}

	for (i = st_idx; i <= end_idx; i++) {
		wkr[i].stop_req = 2;
	}

exit:
	return 0;
}

struct cli_cmd Halt = {
"halt",
2,
1,
"Halt execution of a thread command",
"halt <idx>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) ", or \"all\"\n",
HaltCmd,
ATTR_NONE
};

static int MoveCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;
	int cpu;

	if (gp_parse_worker_index(env, argv[0], &idx)) {
		goto exit;
	}

	if (gp_parse_cpu(env, argv[1], &cpu)) {
		goto exit;
	}

	if (0 == wkr[idx].stat) {
		LOGMSG(env, "\nThread %u was not alive\n", idx);
		goto exit;
	}

	wkr[idx].wkr_thr.cpu_req = cpu;
	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd Move = {
"move",
2,
2,
"Move a thread to a different CPU",
"move <idx> <cpu>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX)",\n" 
	"<cpu> is a cpu number, or -1 to indicate no cpu affinity\n",
MoveCmd,
ATTR_NONE
};

static int WaitCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	const struct timespec ten_usec = {0, 10 * 1000};

	uint16_t idx, limit = 10000;
	int state;

	if (gp_parse_worker_index(env, argv[0], &idx)) {
		goto exit;
	}

	switch (argv[1][0]) {
	case '0':
	case 'd':
	case 'D':
		state = 0;
		break;
	case '1':
	case 'r':
	case 'R':
		state = 1;
		break;
	case '2':
	case 'h':
	case 'H':
		state = 2;
		break;
	default:
		state = -1;
		break;
	}

	if (-1 == state) {
		LOGMSG(env, "\nState must be 0|d|D, 1|r|R, or 2|h|H\n");
		goto exit;
	}

	while ((wkr[idx].stat != state) && limit--) {
		time_sleep(&ten_usec);
	}

	if (wkr[idx].stat == state) {
		LOGMSG(env, "\nPassed, Worker %u is now %s\n", idx,
				THREAD_STR(wkr[idx].stat));
	} else {
		LOGMSG(env, "\nFAILED, Worker %u is now %s\n", idx,
				THREAD_STR(wkr[idx].stat));
	}

exit:
	return 0;
}

struct cli_cmd Wait = {
"wait",
2,
2,
"Wait until a thread reaches a particular state",
"wait <idx> <state>\n"
	"<idx>   is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<state> 0|d|D - Dead, 1|r|R - Run, 2|h|H - Halted\n",
WaitCmd,
ATTR_NONE
};

static int SleepCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	float sec;
	double fractional;
	double seconds;
	struct timespec delay;

	sec = GetFloatParm(argv[0], 0);
	if(sec > 0) {
		LOGMSG(env, "\nSleeping %f sec\n", sec);
		fractional = modf(sec, &seconds);
		delay.tv_sec = seconds;
		delay.tv_nsec = fractional * 1000000 * 1000;
		time_sleep(&delay);
	}
	return 0;
}

struct cli_cmd Sleep = {
"sleep",
2,
1,
"Sleep for a number of seconds (fractional allowed)",
"sleep <sec>\n"
	"<sec> is the number of seconds to sleep\n",
SleepCmd,
ATTR_NONE
};

#define FOUR_KB (4*1024)
#define SIXTEEN_MB (16*1024*1024)

static int IBAllocCmd(struct cli_env *env, int argc, char **argv)
{
	uint16_t idx;
	uint64_t ib_size;
	uint64_t ib_rio_addr = RIO_ANY_ADDR;
	uint64_t ib_phys_addr= RIO_ANY_ADDR;

	if (gp_parse_worker_index_check_thread(env, argv[0], &idx, 1)) {
		goto exit;
	}

	if (gp_parse_ull_pw2(env, argv[1], "<size>", &ib_size, FOUR_KB,
			4 * SIXTEEN_MB)) {
		goto exit;
	}

	/* Note: RSVD overrides rio_addr */
	if (argc > 3) {
		if (get_rsvd_phys_mem(argv[3], &ib_phys_addr, &ib_size)) {
			LOGMSG(env, "\nNo reserved memory found for keyword %s",
					argv[3]);
			goto exit;
		}
	} else if ((argc > 2)
			&& (tok_parse_ulonglong(argv[2], &ib_rio_addr, 1,
					UINT64_MAX, 0))) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, "<addr>",
				(uint64_t )1, (uint64_t)UINT64_MAX);
		goto exit;
	}

	if ((ib_rio_addr != RIO_ANY_ADDR ) && ((ib_size - 1) & ib_rio_addr)) {
		LOGMSG(env, "\n<addr> not aligned with <size>\n");
		goto exit;
	}

	wkr[idx].action = alloc_ibwin;
	wkr[idx].ib_byte_cnt = ib_size;
	wkr[idx].ib_rio_addr = ib_rio_addr;
	wkr[idx].ib_handle = ib_phys_addr;
	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd IBAlloc = {
"IBAlloc",
3,
2,
"Allocate an inbound window",
"IBAlloc <idx> <size> {<addr> {<RSVD>}}\n"
	"<size> must be a power of two from 0x1000 to 0x01000000\n"
	"<addr> is the optional RapidIO address for the inbound window\n"
	"       NOTE: <addr> must be aligned to <size>\n"
	"<RSVD> is a keyword for reserved memory area\n"
	"       NOTE: If <RSVD> is specified, <addr> is ignored\n",
IBAllocCmd,
ATTR_NONE
};

static int IBDeallocCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;

	if (gp_parse_worker_index_check_thread(env, argv[0], &idx, 1)) {
		goto exit;
	}

	wkr[idx].action = free_ibwin;
	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd IBDealloc = {
"IBDealloc",
3,
1,
"Deallocate an inbound window",
"IBDealloc <idx>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n",
IBDeallocCmd,
ATTR_NONE
};

#define PROC_STAT_PFMT "\nTot CPU Jiffies %lu %lu %lu %lu %lu %lu %lu\n"

#define CPUOCC_BUFF_SIZE 1024

static void cpu_occ_parse_proc_line(char *file_line, uint64_t *proc_new_utime,
		uint64_t *proc_new_stime)
{
	char *tok;
	char *saveptr;
	char *delim = (char *)" ";
	int tok_cnt = 0;
	char fl_cpy[CPUOCC_BUFF_SIZE];
	
	SAFE_STRNCPY(fl_cpy, file_line, sizeof(fl_cpy));
	tok = strtok_r(file_line, delim, &saveptr);
	while ((NULL != tok) && (tok_cnt < 13)) {
		tok = strtok_r(NULL, delim, &saveptr);
		tok_cnt++;
	}

	if (NULL == tok) {
		goto error;
	}
	if (tok_parse_ull(tok, proc_new_utime, 0)) {
		goto error;
	}

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok) {
		goto error;
	}
	if (tok_parse_ull(tok, proc_new_stime, 0)) {
		goto error;
	}
	return;

error:
	ERR("\nFAILED: proc_line \"%s\"\n", fl_cpy);
}

static void cpu_occ_parse_stat_line(char *file_line, uint64_t *p_user,
		uint64_t *p_nice, uint64_t *p_system, uint64_t *p_idle,
		uint64_t *p_iowait, uint64_t *p_irq, uint64_t *p_softirq)
{
	char *tok, *saveptr;
	char *delim = (char *)" ";
	char fl_cpy[CPUOCC_BUFF_SIZE];
	
	SAFE_STRNCPY(fl_cpy, file_line, sizeof(fl_cpy));
	
	/* Throw the first token away. */
	tok = strtok_r(file_line, delim, &saveptr);
	if (NULL == tok)
		goto error;
	
	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ull(tok, p_user, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ull(tok, p_nice, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ull(tok, p_system, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ull(tok, p_idle, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ull(tok, p_iowait, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ull(tok, p_irq, 0))
		goto error;

	tok = strtok_r(NULL, delim, &saveptr);
	if (NULL == tok)
		goto error;
	if (tok_parse_ull(tok, p_softirq, 0))
		goto error;
	
	return;
error:
	ERR("\nFAILED: stat_line \"%s\"\n", fl_cpy);
}

int cpu_occ_valid;

uint64_t old_tot_jifis;
uint64_t old_proc_kern_jifis;
uint64_t old_proc_user_jifis;
uint64_t new_tot_jifis;
uint64_t new_proc_kern_jifis;
uint64_t new_proc_user_jifis;

float cpu_occ_pct;

static int cpu_occ_set(uint64_t *tot_jifis, uint64_t *proc_kern_jifis,
		uint64_t *proc_user_jifis)
{
	FILE *stat_fp = NULL, *cpu_stat_fp = NULL;
	char filename[256] = {0};
	char file_line[CPUOCC_BUFF_SIZE] = {0};
	uint64_t p_user = 1, p_nice = 1, p_system = 1, p_idle = 1;
	uint64_t p_iowait = 1, p_irq = 1, p_softirq = 1;

	pid_t my_pid = getpid();

	snprintf(filename, 255, "/proc/%d/stat", my_pid);

	stat_fp = fopen(filename, "re" );
	if (NULL == stat_fp) {
		ERR( "FAILED: Open proc stat file \"%s\": %d %s\n",
			filename, errno, strerror(errno));
		goto exit;
	}

	cpu_stat_fp = fopen("/proc/stat", "re");
	if (NULL == cpu_stat_fp) {
		ERR("FAILED: Open file \"/proc/stat\": %d %s\n",
			errno, strerror(errno));
		goto exit;
	}

	if (NULL == fgets(file_line, sizeof(file_line), stat_fp)) {
		ERR("Unexpected EOF 1: %d %s\n", errno, strerror(errno));
		goto exit;
	}

	cpu_occ_parse_proc_line(file_line, proc_user_jifis, proc_kern_jifis);

		
	memset(file_line, 0, sizeof(file_line));
	if (NULL == fgets(file_line, sizeof(file_line), cpu_stat_fp)) {
		ERR("Unexpected EOF 2: %d %s\n", errno, strerror(errno));
		goto exit;
	}

	cpu_occ_parse_stat_line(file_line, &p_user, &p_nice, &p_system,
			&p_idle, &p_iowait, &p_irq, &p_softirq);

	*tot_jifis = p_user + p_nice + p_system + p_idle +
			p_iowait + p_irq + p_softirq;

exit:
	if (stat_fp != NULL) {
		fclose(stat_fp);
	}
	if (cpu_stat_fp != NULL) {
		fclose(cpu_stat_fp);
	}
	return 0;
}

static int CPUOccSetCmd(struct cli_env *env, int UNUSED(argc),
		char **UNUSED(argv))
{

	if (cpu_occ_set(&old_tot_jifis, &old_proc_kern_jifis,
			&old_proc_user_jifis)) {
		LOGMSG(env, "\nFAILED: Could not get proc info \n");
		goto exit;
	}
	LOGMSG(env, "\nSet CPU Occ measurement start point\n");

	cpu_occ_valid = 1;

exit:
	return 0;
}

struct cli_cmd CPUOccSet = {
"oset",
2,
0,
"Set CPU Occupancy measurement start point.",
"oset\n"
	"No parameters\n", 
CPUOccSetCmd,
ATTR_NONE
};

int cpu_occ_saved_idx;

static int CPUOccDisplayCmd(struct cli_env *env, int UNUSED(argc),
		char **UNUSED(argv))
{
	char pctg[FLOAT_STR_SIZE];
	int cpus = getCPUCount();

	if (!cpus) {
		cpus = 1;
	}

	if (!cpu_occ_valid) {
		LOGMSG(env, "\nFAILED: CPU OCC measurement start not set\n");
		goto exit;
	}

	if (cpu_occ_set(&new_tot_jifis, &new_proc_kern_jifis,
			&new_proc_user_jifis)) {
		LOGMSG(env, "\nFAILED: Could not get proc info \n");
		goto exit;
	}

	cpu_occ_pct = (((float)(new_proc_kern_jifis + new_proc_user_jifis
			- old_proc_kern_jifis - old_proc_user_jifis))
			/ ((float)(new_tot_jifis - old_tot_jifis))) * 100.0
			* cpus;
	snprintf(pctg, sizeof(pctg), "%4.2f", cpu_occ_pct);

	LOGMSG(env, "\n-Kernel- ProcUser ProcKern CPU_Occ\n");
	LOGMSG(env, "%8ld %8ld %8ld %7s\n", new_tot_jifis - old_tot_jifis,
			new_proc_user_jifis - old_proc_user_jifis,
			new_proc_kern_jifis - old_proc_kern_jifis, pctg);

exit:
	return 0;
}

struct cli_cmd CPUOccDisplay = {
"odisp",
2,
0,
"Display cpu occupancy",
"odisp\n"
	"No parameters\n",
CPUOccDisplayCmd,
ATTR_RPT
};

static int obdio_cmd(struct cli_env *env, int UNUSED(argc), char **argv,
		enum req_type action)
{
	uint16_t idx;
	did_val_t did_val;
	uint64_t rio_addr;
	uint64_t bytes;
	uint64_t acc_sz;
	uint16_t wr;
	uint32_t min_obwin_size = 0x10000;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (gp_parse_did(env, argv[n++], &did_val)) {
		goto exit;
	}

	if (tok_parse_ulonglong(argv[n++], &rio_addr, 1, UINT64_MAX, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, "<rio_addr>",
				(uint64_t)1, (uint64_t)UINT64_MAX);
		goto exit;
	}

	if (direct_io == action) {
		if (tok_parse_ull(argv[n++], &bytes, 0)) {
			LOGMSG(env, "\n");
			LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<bytes>");
			goto exit;
		}
		if (gp_parse_ull_pw2(env, argv[n++], "<acc_sz>", &acc_sz, 1, 8)) {
			goto exit;
		}
		if (gp_parse_bool(env, argv[n++], "<wr>", &wr)) {
			goto exit;
		}
	} else {
		if (gp_parse_ull_pw2(env, argv[n++], "<acc_sz>", &acc_sz, 1, 8)) {
			goto exit;
		}
		bytes = acc_sz;
		if (direct_io_tx_lat == action) {
			if (gp_parse_bool(env, argv[n++], "<wr>", &wr)) {
				goto exit;
			}
		} else {
			wr = 1;
		}
	}

	if ((direct_io_tx_lat == action)
			&& (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr))) {
		LOGMSG(env, "\nNo mapped inbound window present\n");
		goto exit;
	}

	wkr[idx].action = action;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = did_val;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].wr = wr;
	// Set up the size of the outbound window, which must be:
	// - at least 0x10000 bytes
	// - larger than the bytes to transfer,
	// - a power of 2.
	wkr[idx].ob_byte_cnt = min_obwin_size;
	if ((direct_io == action) && (bytes > min_obwin_size)) {
		wkr[idx].ob_byte_cnt = roundup_pw2(bytes);
		if (!wkr[idx].ob_byte_cnt) { 
			LOGMSG(env, "\nInvalid outbound window size\n");
			goto exit;
		}
	}

	if (bytes % acc_sz) {
		LOGMSG(env, "\nBytes must be a multiple of acc_sz\n");
		goto exit;
	}

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

static int OBDIOCmd(struct cli_env *env, int argc, char **argv)
{
	return obdio_cmd(env, argc, argv, direct_io);
}

struct cli_cmd OBDIO = {
"OBDIO",
5,
6,
"Measure goodput of reads/writes through an outbound window",
"OBDIO <idx> <did> <rio_addr> <bytes> <acc_sz> <wr>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes>    total bytes to transfer, must be a multiple of <acc_sz>\n"
	"<acc_sz>   access size, 1, 2, 4, 8\n"
	"<wr>       0: Read, 1: Write\n",
OBDIOCmd,
ATTR_NONE
};

static int OBDIOTxLatCmd(struct cli_env *env, int argc, char **argv)
{
	return obdio_cmd(env, argc, argv, direct_io_tx_lat);
}

struct cli_cmd OBDIOTxLat = {
"DIOTxLat",
8,
5,
"Measure latency of reads/writes through an outbound window",
"DIOTxLat <idx> <did> <rio_addr> <acc_sz> <wr>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<acc_sz>   access size, values: 1, 2, 4, 8\n"
	"<wr>       0: Read, 1: Write\n"
	"           NOTE: For <wr> = 1, there must be a <did> thread running OBDIORxLat!\n",
OBDIOTxLatCmd,
ATTR_NONE
};

static int OBDIORxLatCmd(struct cli_env *env, int argc, char **argv)
{
	return obdio_cmd(env, argc, argv, direct_io_rx_lat);
}

struct cli_cmd OBDIORxLat = {
"DIORxLat",
4,
4,
"Loop back DIOTxLat writes through an outbound window",
"DIORxLat <idx> <did> <rio_addr> <acc_sz>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<acc_sz>   access size, 1, 2, 4 or 8\n"
	"\nNOTE: DIORxLat must be run before OBDIOTxLat!\n",
OBDIORxLatCmd,
ATTR_NONE
};

// "<trans>  0 NW, 1 SW, 2 NW_R, 3 SW_R 4 NW_R_ALL\n"
enum riomp_dma_directio_type convert_int_to_riomp_dma_directio_type(uint16_t trans)
{
	switch (trans) {
	default:
	case 0:
		return RIO_DIRECTIO_TYPE_NWRITE;
	case 1:
		return RIO_DIRECTIO_TYPE_SWRITE;
	case 2:
		return RIO_DIRECTIO_TYPE_NWRITE_R;
	case 3:
		return RIO_DIRECTIO_TYPE_SWRITE_R;
	case 4:
		return RIO_DIRECTIO_TYPE_NWRITE_R_ALL;
	}
}

static int dmaCmd(struct cli_env *env, int argc, char **argv)
{
	uint16_t idx;
	did_val_t did_val;
	uint64_t rio_addr;
	uint64_t bytes;
	uint64_t acc_sz;
	uint16_t wr;
	uint16_t kbuf;
	uint16_t trans;
	uint16_t sync;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (gp_parse_did(env, argv[n++], &did_val)) {
		goto exit;
	}

	if (tok_parse_ulonglong(argv[n++], &rio_addr, 1, UINT64_MAX, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, "<rio_addr>",
				(uint64_t)1, (uint64_t)UINT64_MAX);
		goto exit;
	}

	if (tok_parse_ull(argv[n++], &bytes, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<bytes>");
		goto exit;
	}

	if (gp_parse_ull_pw2(env, argv[n++], "<acc_sz>", &acc_sz, 1, UINT32_MAX)) {
		goto exit;
	}

	if (gp_parse_bool(env, argv[n++], "<wr>", &wr)) {
		goto exit;
	}

	if (gp_parse_bool(env, argv[n++], "<kbuf>", &kbuf)) {
		goto exit;
	}

	if (tok_parse_ushort(argv[n++], &trans, 0, 4, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<trans>", 0, 4);
		goto exit;
	}

	if (tok_parse_ushort(argv[n++], &sync, 0, 2, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<sync>", 0, 2);
		goto exit;
	}

	// Optional parameters - ssdist, sssize, dsdist, dssize
	wkr[idx].ssdist = 0;
	wkr[idx].sssize = 0;
	wkr[idx].dsdist = 0;
	wkr[idx].dssize = 0;

	if ((argc > 9)
			&& (tok_parse_ushort(argv[n++], &wkr[idx].ssdist, 0,
					0xFFFF, 0))) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<ssdist>", 0, 0xFFFF);
		goto exit;
	}

	if ((argc > 10)
			&& (tok_parse_ushort(argv[n++], &wkr[idx].sssize, 0,
					0x0FFF, 0))) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<sssize>", 0, 0x0FFF);
		goto exit;
	}

	if ((argc > 11)
			&& (tok_parse_ushort(argv[n++], &wkr[idx].dsdist, 0,
					0xFFFF, 0))) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<dsdist>", 0, 0xFFFF);
		goto exit;
	}

	if ((argc > 12)
			&& (tok_parse_ushort(argv[n++], &wkr[idx].dssize, 0,
					0x0FFF, 0))) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<dssize>", 0, 0x0FFF);
		goto exit;
	}

	wkr[idx].action = dma_tx;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = did_val;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].wr = wr;
	wkr[idx].use_kbuf = kbuf;
	wkr[idx].dma_trans_type = convert_int_to_riomp_dma_directio_type(trans);
	wkr[idx].dma_sync_type = (enum riomp_dma_directio_transfer_sync)sync;
	wkr[idx].rdma_buff_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd dma = {
"dma",
3,
9,
"Measure goodput of DMA reads/writes",
"dma <idx> <did> <rio_addr> <bytes> <acc_sz> <wr> <kbuf> <trans> <sync> [<ssdist> <sssize> <dsdist> <dssize>]\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes>    total bytes to transfer\n"
	"<acc_sz>   access size, must be a power of two from 1 to 0xffffffff\n"
	"<wr>       0: Read, 1: Write\n"
	"<kbuf>     0: User memory, 1: Kernel buffer\n"
	"<trans>    0: NW, 1: SW, 2: NW_R, 3: SW_R, 4: NW_R_ALL\n"
	"<sync>     0: SYNC, 1: ASYNC, 2: FAF\n"
	"<ssdist>   source stride distance (Optional, default to 0)\n"
	"<sssize>   source stride size (Optional, default to 0)\n"
	"<dsdist>   destination stride distance (Optional, default to 0)\n"
	"<dssize>   destination stride size (Optional, default to 0)\n",
dmaCmd,
ATTR_NONE
};

static int dmaNumCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;
	did_val_t did_val;
	uint64_t rio_addr;
	uint64_t bytes;
	uint64_t acc_sz;
	uint16_t wr;
	uint16_t kbuf;
	uint16_t trans;
	uint16_t sync;
	uint32_t num_trans;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (gp_parse_did(env, argv[n++], &did_val)) {
		goto exit;
	}

	if (tok_parse_ulonglong(argv[n++], &rio_addr, 1, UINT64_MAX, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, "<rio_addr>",
				(uint64_t)1, (uint64_t)UINT64_MAX);
		goto exit;
	}

	if (tok_parse_ull(argv[n++], &bytes, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<bytes>");
		goto exit;
	}

	if (gp_parse_ull_pw2(env, argv[n++], "<acc_sz>", &acc_sz, 1, UINT32_MAX)) {
		goto exit;
	}

	if (gp_parse_bool(env, argv[n++], "<wr>", &wr)) {
		goto exit;
	}

	if (gp_parse_bool(env, argv[n++], "<kbuf>", &kbuf)) {
		goto exit;
	}

	if (tok_parse_ushort(argv[n++], &trans, 0, 4, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<trans>", 0, 4);
		goto exit;
	}

	if (tok_parse_ushort(argv[n++], &sync, 0, 2, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<sync>", 0, 2);
		goto exit;
	}

	if (tok_parse_ul(argv[n++], &num_trans, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_UL_HEX_MSG_FMT, "<num>");
		goto exit;
	}

	wkr[idx].action = dma_tx_num;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = did_val;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = acc_sz;
	wkr[idx].wr = (int)wr;
	wkr[idx].use_kbuf = (int)kbuf;
	wkr[idx].dma_trans_type = convert_int_to_riomp_dma_directio_type(trans);
	wkr[idx].dma_sync_type = (enum riomp_dma_directio_transfer_sync)sync;
	wkr[idx].rdma_buff_size = bytes;
	wkr[idx].num_trans = (int)num_trans;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd dmaNum = {
"dnum",
3,
10,
"Send a specified number of DMA reads/writes",
"dnum <idx> <did> <rio_addr> <bytes> <acc_sz> <wr> <kbuf> <trans> <sync> <num>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes>    total bytes to transfer\n"
	"<acc_sz>   access size, must be a power of two from 1 to 0xffffffff\n"
	"<wr>       0: Read, 1: Write\n"
	"<kbuf>     0: User memory, 1: Kernel buffer\n"
	"<trans>    0: NW, 1: SW, 2: NW_R, 3: SW_R, 4: NW_R_ALL\n"
	"<sync>     0: SYNC, 1: ASYNC, 2: FAF\n"
	"<num>      number of transactions to send\n",
dmaNumCmd,
ATTR_NONE
};

static int dmaTxLatCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;
	did_val_t did_val;
	uint64_t rio_addr;
	uint64_t bytes;
	uint16_t wr;
	uint16_t kbuf;
	uint16_t trans;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (gp_parse_did(env, argv[n++], &did_val)) {
		goto exit;
	}

	if (tok_parse_ulonglong(argv[n++], &rio_addr, 1, UINT64_MAX, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, "<rio_addr>",
				(uint64_t)1, (uint64_t)UINT64_MAX);
		goto exit;
	}

	if (tok_parse_ull(argv[n++], &bytes, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<bytes>");
		goto exit;
	}

	if (gp_parse_bool(env, argv[n++], "<wr>", &wr)) {
		goto exit;
	}

	if (gp_parse_bool(env, argv[n++], "<kbuf>", &kbuf)) {
		goto exit;
	}

	if (tok_parse_ushort(argv[n++], &trans, 0, 4, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "<trans>", 0, 4);
		goto exit;
	}

	if (wr && (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr))) {
		LOGMSG(env, "\nNo mapped inbound window present\n");
		goto exit;
	}

	wkr[idx].action = dma_tx_lat;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = did_val;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = bytes;
	wkr[idx].wr = wr;
	wkr[idx].use_kbuf = kbuf;
	wkr[idx].dma_trans_type = convert_int_to_riomp_dma_directio_type(trans);
	wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;

	if (bytes < MIN_RDMA_BUFF_SIZE) {
		wkr[idx].rdma_buff_size = MIN_RDMA_BUFF_SIZE;
	} else {
		wkr[idx].rdma_buff_size = bytes;
	}

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd dmaTxLat = {
"dTxLat",
2,
7,
"Measure lantecy of DMA reads/writes",
"dTxLat <idx> <did> <rio_addr> <bytes> <wr> <kbuf> <trans>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes>    total bytes to transfer\n"
	"<wr>       0: Read, 1: Write\n"
	"           NOTE: For <wr> = 1, there must be a thread on <did> running dRxLat!\n"
	"<kbuf>     0: User memory, 1: Kernel buffer\n"
	"<trans>    0: NW, 1: SW, 2: NW_R, 3: SW_R, 4: NW_R_ALL\n",
dmaTxLatCmd,
ATTR_NONE
};

static int dmaRxLatCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;
	did_val_t did_val;
	uint64_t rio_addr;
	uint64_t bytes;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (gp_parse_did(env, argv[n++], &did_val)) {
		goto exit;
	}

	if (tok_parse_ulonglong(argv[n++], &rio_addr, 1, UINT64_MAX, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, "<rio_addr>",
				(uint64_t)1, (uint64_t)UINT64_MAX);
		goto exit;
	}

	if (tok_parse_ulonglong(argv[n++], &bytes, 1, UINT32_MAX, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONGLONG_HEX_MSG_FMT, "<bytes>",
				(uint64_t )1, (uint64_t)UINT32_MAX);
		goto exit;
	}

	wkr[idx].action = dma_rx_lat;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = did_val;
	wkr[idx].rio_addr = rio_addr;
	wkr[idx].byte_cnt = bytes;
	wkr[idx].acc_size = bytes;
	wkr[idx].wr = 1;
	wkr[idx].use_kbuf = 1;
	wkr[idx].dma_trans_type = RIO_DIRECTIO_TYPE_NWRITE;
	wkr[idx].dma_sync_type = RIO_DIRECTIO_TRANSFER_SYNC;

	if (bytes < MIN_RDMA_BUFF_SIZE) {
		wkr[idx].rdma_buff_size = MIN_RDMA_BUFF_SIZE;
	} else {
		wkr[idx].rdma_buff_size = bytes;
	}

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd dmaRxLat = {
"dRxLat",
2,
4,
"Loop back DMA writes for dTxLat command.",
"dRxLat <idx> <did> <rio_addr> <bytes>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<rio_addr> RapidIO memory address to access\n"
	"<bytes>    total bytes to transfer\n"
	"\nNOTE: The dRxLat command must be run before dTxLat!\n",
dmaRxLatCmd,
ATTR_NONE
};

static void roundoff_message_size(uint32_t *bytes)
{
	if (*bytes > 4096) {
		*bytes = 4096;
	}

	if (*bytes < 24) {
		*bytes = 24;
	}

	*bytes = (*bytes + 7) & 0x1FF8;
}

static int msg_tx_cmd(struct cli_env *env, int UNUSED(argc), char **argv, enum req_type req)
{
	uint16_t idx;
	did_val_t did_val;
	uint16_t sock_num;
	uint32_t bytes;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (gp_parse_did(env, argv[n++], &did_val)) {
		goto exit;
	}

	if (tok_parse_socket(argv[n++], &sock_num, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_SOCKET_MSG_FMT, "<sock_num>");
		goto exit;
	}

	if (tok_parse_ulong(argv[n++], &bytes, CM_HEADER_BYTES, RIO_MAX_MSG_SIZE,
			0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONG_HEX_MSG_FMT, "<size>", CM_HEADER_BYTES,
				RIO_MAX_MSG_SIZE);
		goto exit;
	}

	roundoff_message_size(&bytes);

	wkr[idx].action = req;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = did_val;
	wkr[idx].sock_num = sock_num;
	wkr[idx].msg_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

static int msgTxCmd(struct cli_env *env, int argc, char **argv)
{
	return msg_tx_cmd(env, argc, argv, message_tx);
}

struct cli_cmd msgTx = {
"msgTx",
4,
4,
"Measure goodput of channelized messages",
"msgTx <idx> <did> <sock_num> <size>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size>     bytes per message. Must be a multiple of 8 from 24 to 4096\n"
	"\nNOTE: msgTx must send to a corresponding msgRx!\n",
msgTxCmd,
ATTR_NONE
};

static int msgTxLatCmd(struct cli_env *env, int argc, char **argv)
{
	return msg_tx_cmd(env, argc, argv, message_tx_lat);
}

struct cli_cmd msgTxLat = {
"mTxLat",
2,
4,
"Measures latency of channelized messages",
"mTxLat <idx> <did> <sock_num> <size>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size>     bytes per message. Must be a multiple of 8 from 24 to 4096\n"
	"\nNOTE: mTxLat must be sending to a node running mRxLat!\n"
	"NOTE: mRxLat must be run before mTxLat!\n",
msgTxLatCmd,
ATTR_NONE
};

static int msgRxCmdExt(struct cli_env *env, int UNUSED(argc), char **argv, enum req_type action)
{
	uint16_t idx;
	uint16_t sock_num;
	uint32_t bytes;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (tok_parse_socket(argv[n++], &sock_num, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_SOCKET_MSG_FMT, "<sock_num>");
		goto exit;
	}

	if (tok_parse_ulong(argv[n++], &bytes, CM_HEADER_BYTES, RIO_MAX_MSG_SIZE,
			0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULONG_HEX_MSG_FMT, "<size>", CM_HEADER_BYTES,
				RIO_MAX_MSG_SIZE);
		goto exit;
	}

	roundoff_message_size(&bytes);

	wkr[idx].action = action;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = 0;
	wkr[idx].sock_num = sock_num;
	wkr[idx].msg_size = bytes;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

static int msgRxLatCmd(struct cli_env *env, int argc, char **argv)
{
	return msgRxCmdExt(env, argc, argv, message_rx_lat);
}

struct cli_cmd msgRxLat = {
"mRxLat",
2,
3,
"Loops back received messages to mTxLat sender",
"mRxLat <idx> <sock_num> <size>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<sock_num> RapidIO Channelized Messaging channel number to accept\n"
	"<size>     bytes per message. Must be a multiple of 8 from 24 to 4096\n"
	"\nNOTE: mRxLat must be run before mTxLat!\n",
msgRxLatCmd,
ATTR_NONE
};

static int msgRxCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;
	uint16_t sock_num;

	int n = 0;
	if (gp_parse_worker_index_check_thread(env, argv[n++], &idx, 1)) {
		goto exit;
	}

	if (tok_parse_socket(argv[n++], &sock_num, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_SOCKET_MSG_FMT, "<sock_num>");
		goto exit;
	}

	wkr[idx].action = message_rx;
	wkr[idx].action_mode = kernel_action;
	wkr[idx].did_val = 0;
	wkr[idx].sock_num = sock_num;

	wkr[idx].stop_req = 0;
	sem_post(&wkr[idx].run);

exit:
	return 0;
}

struct cli_cmd msgRx = {
"msgRx",
4,
2,
"Receives channelized messages as requested",
"msgRx <idx> <sock_num>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<sock_num> target socket number for connections from msgTx command\n"
	"\nNOTE: msgRx must be running before msgTx!\n",
msgRxCmd,
ATTR_NONE
};

static int msgTxOhCmd(struct cli_env *env, int argc, char **argv)
{
	return msg_tx_cmd(env, argc, argv, message_tx_oh);
}

struct cli_cmd msgTxOh = {
"mTxOh",
4,
4,
"Measures overhead of channelized messages",
"mTxOh <idx> <did> <sock_num> <size>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<did>      target device ID\n"
	"<sock_num> RapidIO Channelized Messaging channel number to connect\n"
	"<size>     bytes per message. Must be a multiple of 8 from 24 to 4096\n"
	"\nNOTE: mTxOh must be sending to a node running mRxOh!\n"
	"NOTE: mRxOh must be run before mTxOh!\n",
msgTxOhCmd,
ATTR_NONE
};

static int msgRxOhCmd(struct cli_env *env, int argc, char **argv)
{
	return msgRxCmdExt(env, argc, argv, message_rx_oh);
}

struct cli_cmd msgRxOh = {
"mRxOh",
4,
3,
"Loops back received messages to mTxOh sender",
"mRxOh <idx> <sock_num> <size>\n"
	"<idx>      is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<sock_num> RapidIO Channelized Messaging channel number to accept\n"
	"<size>     bytes per message. Must be a multiple of 8 from 24 to 4096\n"
	"\nNOTE: mRxOh must be run before mTxOh!\n",
msgRxOhCmd,
ATTR_NONE
};

static int GoodputCmd(struct cli_env *env, int argc, char **UNUSED(argv))
{
	int i;
	float MBps;
	float Gbps;
	float Msgpersec;
	float link_occ;
	uint64_t byte_cnt;
	float tot_MBps = 0;
	float tot_Gbps = 0;
	float tot_Msgpersec = 0;
	uint64_t tot_byte_cnt = 0;
	char MBps_str[FLOAT_STR_SIZE];
	char Gbps_str[FLOAT_STR_SIZE];
	char link_occ_str[FLOAT_STR_SIZE];

	LOGMSG(env, "\n W STS <<<<--Data-->>>> --MBps-- -Gbps- Messages  Link_Occ\n");

	for (i = 0; i < MAX_WORKERS; i++) {
		struct timespec elapsed;
		uint64_t nsec;

		Msgpersec = wkr[i].perf_msg_cnt;
		byte_cnt = wkr[i].perf_byte_cnt;

		elapsed = time_difference(wkr[i].st_time, wkr[i].end_time);
		nsec = elapsed.tv_nsec + (elapsed.tv_sec * 1000000000);

		MBps = (float)(byte_cnt / (1024*1024)) / 
			((float)nsec / 1000000000.0);
		Gbps = (MBps * 1024.0 * 1024.0 * 8.0) / 1000000000.0;
		link_occ = Gbps/0.95;

		memset(MBps_str, 0, FLOAT_STR_SIZE);
		memset(Gbps_str, 0, FLOAT_STR_SIZE);
		memset(link_occ_str, 0, FLOAT_STR_SIZE);
		snprintf(MBps_str, sizeof(MBps_str), "%4.3f", MBps);
		snprintf(Gbps_str, sizeof(Gbps_str), "%2.3f", Gbps);
		snprintf(link_occ_str, sizeof(link_occ_str), "%2.3f", link_occ);

		LOGMSG(env, "%2d %3s %16lx %8s %6s %9.0f  %6s\n", i,
				THREAD_STR(wkr[i].stat), byte_cnt, MBps_str,
				Gbps_str, Msgpersec, link_occ_str);

		if (byte_cnt) {
			tot_byte_cnt += byte_cnt;
			tot_MBps += MBps;
			tot_Gbps += Gbps;
		}
		tot_Msgpersec += Msgpersec;

		if (argc) {
			wkr[i].perf_byte_cnt = 0;
			wkr[i].perf_msg_cnt = 0;
			clock_gettime(CLOCK_MONOTONIC, &wkr[i].st_time);
		}
	}

	link_occ = tot_Gbps/0.95;
	memset(MBps_str, 0, FLOAT_STR_SIZE);
	memset(Gbps_str, 0, FLOAT_STR_SIZE);
	memset(link_occ_str, 0, FLOAT_STR_SIZE);
	snprintf(MBps_str, sizeof(MBps_str), "%4.3f", tot_MBps);
	snprintf(Gbps_str, sizeof(Gbps_str), "%2.3f", tot_Gbps);
	snprintf(link_occ_str, sizeof(link_occ_str), "%2.3f", link_occ);
	LOGMSG(env, "Total  %16lx %8s %6s %9.0f  %6s\n", tot_byte_cnt, MBps_str,
			Gbps_str, tot_Msgpersec, link_occ_str);

	return 0;
}

struct cli_cmd Goodput = {
"goodput",
1,
0,
"Print current performance for threads.",
"goodput {<optional>}\n"
	"Any parameter to goodput causes the byte and message counts of all\n"
	"   running threads to be zeroed after they are displayed\n",
GoodputCmd,
ATTR_RPT
};

static int LatCmd(struct cli_env *env, int UNUSED(argc), char **UNUSED(argv))
{
	int i;
	char min_lat_str[FLOAT_STR_SIZE];
	char avg_lat_str[FLOAT_STR_SIZE];
	char max_lat_str[FLOAT_STR_SIZE];

	LOGMSG(env, "\n W STS <<<<-Count-->>>> <<<<Min uSec>>>> <<<<Avg uSec>>>> <<<<Max uSec>>>>\n");

	for (i = 0; i < MAX_WORKERS; i++) {
		uint64_t tot_nsec;
		uint64_t avg_nsec;
		uint64_t divisor;

		divisor = (wkr[i].wr)?2:1;

		tot_nsec = wkr[i].tot_iter_time.tv_nsec +
				(wkr[i].tot_iter_time.tv_sec * 1000000000);

		/* Note: divide by 2 to account for round trip latency. */
		if (wkr[i].perf_iter_cnt) {
			avg_nsec = tot_nsec/divisor/wkr[i].perf_iter_cnt;
		} else {
			avg_nsec = 0;
		}

		memset(min_lat_str, 0, FLOAT_STR_SIZE);
		memset(avg_lat_str, 0, FLOAT_STR_SIZE);
		memset(max_lat_str, 0, FLOAT_STR_SIZE);
		snprintf(min_lat_str, sizeof(min_lat_str), "%4.3f",
			(float)(wkr[i].min_iter_time.tv_nsec/divisor)/1000.0); 
		snprintf(avg_lat_str, sizeof(avg_lat_str), "%4.3f", (float)avg_nsec/1000.0);
		snprintf(max_lat_str, sizeof(max_lat_str), "%4.3f",
			(float)(wkr[i].max_iter_time.tv_nsec/divisor)/1000.0); 

		LOGMSG(env, "%2d %3s %16ld %16s %16s %16s\n", i,
				THREAD_STR(wkr[i].stat), wkr[i].perf_iter_cnt,
				min_lat_str, avg_lat_str, max_lat_str);
	}

	return 0;
}

struct cli_cmd Lat = {
"lat",
3,
0,
"Print current latency for threads.",
"<No Parameters>\n",
LatCmd,
ATTR_RPT
};


static inline void display_cpu(struct cli_env *env, int cpu)
{
	if (-1 == cpu) {
		LOGMSG(env, "Any ");
		return;
	}
	LOGMSG(env, "%3d ", cpu);
}

static void display_gen_status(struct cli_env *env)
{
	LOGMSG(env, "\n W STS CPU RUN ACTION  MODE DID <<<<--ADDR-->>>> ByteCnt AccSize W H OB IB MB\n");

	for (int i = 0; i < MAX_WORKERS; i++) {
		LOGMSG(env, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		LOGMSG(env, 
			"%7s %4s %3d %16lx %7lx %7lx %1d %1d %2d %2d %2d\n",
			ACTION_STR(wkr[i].action), 
			MODE_STR(wkr[i].action_mode), wkr[i].did_val,
			wkr[i].rio_addr, wkr[i].byte_cnt, wkr[i].acc_size, 
			wkr[i].wr, wkr[i].mp_h_is_mine,
			wkr[i].ob_valid, wkr[i].ib_valid, 
			wkr[i].mb_valid);
	}
}

static void display_ibwin_status(struct cli_env *env)
{
	LOGMSG(env, "\n W STS CPU RUN ACTION  MODE IB <<<< HANDLE >>>> <<<<RIO ADDR>>>> <<<<  SIZE  >>>\n");

	for (int i = 0; i < MAX_WORKERS; i++) {
		LOGMSG(env, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		LOGMSG(env, 
			"%7s %4s %2d %16lx %16lx %15lx\n",
			ACTION_STR(wkr[i].action), 
			MODE_STR(wkr[i].action_mode), 
			wkr[i].ib_valid, wkr[i].ib_handle, wkr[i].ib_rio_addr, 
			wkr[i].ib_byte_cnt);
	}
}

static void display_msg_status(struct cli_env *env)
{
	int i;

	LOGMSG(env,
	"\n W STS CPU RUN ACTION  MODE MB ACC CON Msg_Size SockNum TX RX\n");

	for (i = 0; i < MAX_WORKERS; i++) {
		LOGMSG(env, "%2d %3s ", i, THREAD_STR(wkr[i].stat));
		display_cpu(env, wkr[i].wkr_thr.cpu_req);
		display_cpu(env, wkr[i].wkr_thr.cpu_run);
		LOGMSG(env,
			"%7s %4s %2d %3d %3d %8d %7d %2d %2d\n",
			ACTION_STR(wkr[i].action), 
			MODE_STR(wkr[i].action_mode), 
			wkr[i].mb_valid, wkr[i].acc_skt_valid,
			wkr[i].con_skt_valid, wkr[i].msg_size,
			wkr[i].sock_num, (NULL != wkr[i].sock_tx_buf),
			(NULL != wkr[i].sock_rx_buf)
		)
	}
}

static int StatusCmd(struct cli_env *env, int argc, char **argv)
{
	char sel_stat = 'g';

	if (argc) {
		sel_stat = argv[0][0];
	}
	
	switch (sel_stat) {
		case 'i':
		case 'I': 
			display_ibwin_status(env);
			break;
		case 'm':
		case 'M': 
			display_msg_status(env);
			break;
		case 'g':
		case 'G': 
			display_gen_status(env);
			break;
		default:
			LOGMSG(env, "Unknown option \"%c\"\n", argv[0][0]);
			return 0;
	}

	return 0;
}

struct cli_cmd Status = {
"status",
2,
0,
"Display status of all threads",
"status {i|m|s}\n"
	"Optionally enter a character to select the status type:\n"
	"i : IBWIN status\n"
	"m : Messaging status\n"
	"g : General status\n"
	"Default is general status\n",
StatusCmd,
ATTR_RPT
};

int dump_idx;
uint64_t dump_base_offset;
uint64_t dump_size;

static int DumpCmd(struct cli_env *env, int argc, char **argv)
{
	uint16_t idx;
	uint64_t offset, base_offset;
	uint64_t size;
	int n = 0;

	if (argc) {
		if (gp_parse_worker_index(env, argv[n++], &idx)) {
			goto exit;
		}

		if (tok_parse_ull(argv[n++], &base_offset, 0)) {
			LOGMSG(env, "\n");
			LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<base_offset>");
			goto exit;
		}

		if (tok_parse_ull(argv[n++], &size, 0)) {
			LOGMSG(env, "\n");
			LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<size>");
			goto exit;
		}
	} else {
		idx = dump_idx;
		base_offset = dump_base_offset;
		size = dump_size;
	}

	if (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr)) {
		LOGMSG(env, "\nNo mapped inbound window present\n");
		goto exit;
	}

	if ((base_offset + size) > wkr[idx].ib_byte_cnt) {
		LOGMSG(env, "\nOffset + size exceeds window bytes\n");
		goto exit;
	}

	dump_idx = idx;
	dump_base_offset = base_offset;
	dump_size = size;

	LOGMSG(env,
		"          Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
	for (offset = 0; offset < size; offset++) {
		if (!(offset & 0xF)) {
			LOGMSG(env, "\n%" PRIx64 "", base_offset + offset);
		}
		LOGMSG(env, " %2x",
			*(volatile uint8_t * volatile)(
			(uint8_t *)wkr[idx].ib_ptr + base_offset + offset));
	}
	LOGMSG(env, "\n");

exit:
	return 0;
}

struct cli_cmd Dump = {
"dump",
2,
3,
"Dump inbound memory area",
"Dump <idx> <offset> <size>\n"
	"<idx>    is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<offset> is the hexadecimal offset, in bytes, from the window start\n"
	"<size>   is the number of bytes to display, starting at <offset>\n",
DumpCmd,
ATTR_RPT
};

static int FillCmd(struct cli_env *env, int UNUSED(argc), char **argv)
{
	uint16_t idx;
	uint64_t offset, base_offset;
	uint64_t size;
	uint16_t tmp;
	uint8_t data;

	int n = 0;
	if (gp_parse_worker_index(env, argv[n++], &idx)) {
		goto exit;
	}

	if (tok_parse_ull(argv[n++], &base_offset, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<offset>");
		goto exit;
	}

	if (tok_parse_ull(argv[n++], &size, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "<size>");
		goto exit;
	}

	if (tok_parse_ushort(argv[n++], &tmp, 0, 0xff, 0)) {
		LOGMSG(env, "\n");
		LOGMSG(env, TOK_ERR_USHORT_HEX_MSG_FMT, "<data>", 0, 0xff);
		goto exit;
	}
	data = (uint8_t)tmp;

	if (!wkr[idx].ib_valid || (NULL == wkr[idx].ib_ptr)) {
		LOGMSG(env, "\nNo mapped inbound window present\n");
		goto exit;
	}

	if ((base_offset + size) > wkr[idx].ib_byte_cnt) {
		LOGMSG(env, "\nOffset + size exceeds window bytes\n");
		goto exit;
	}

	dump_idx = idx;
	dump_base_offset = base_offset;
	dump_size = size;

	for (offset = 0; offset < size; offset++) {
		*(volatile uint8_t * volatile)
		((uint8_t *)wkr[idx].ib_ptr + base_offset + offset) = data;
	}

exit:
	return 0;
}

struct cli_cmd Fill = {
"fill",
4,
4,
"Fill inbound memory area",
"Fill <idx> <offset> <size> <data>\n"
	"<idx>    is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<offset> is the offset, in bytes, from the window start\n"
	"<size>   is the number of bytes to display, starting at <offset>\n"
	"<data>   is the 8 bit value to write\n",
FillCmd,
ATTR_RPT
};

static int MpdevsCmd(struct cli_env *env, int UNUSED(argc), char **UNUSED(argv))
{
	mport_list_t *mport_list = NULL;
	mport_list_t *list_ptr;
	uint8_t number_of_mports = RIO_MAX_MPORTS;
	uint8_t mport_id;

	did_val_t *ep_list = NULL;
	uint32_t number_of_eps = 0;
	uint32_t ep;
	int i;
	int ret;

	ret = riomp_mgmt_get_mport_list(&mport_list, &number_of_mports);
	if (ret) {
		LOGMSG(env, "riomp_mgmt_get_mport_list ERR %d:%s\n", ret,
				strerror(ret));
		goto exit;
	}

	LOGMSG(env, "\nAvailable %d local mport(s):\n", number_of_mports);

	if (number_of_mports > RIO_MAX_MPORTS) {
		LOGMSG(env, "WARNING: Only %d out of %d have been retrieved\n",
				RIO_MAX_MPORTS, number_of_mports);
	}

	list_ptr = mport_list;
	for (i = 0; i < number_of_mports; i++, list_ptr++) {
		mport_id = *list_ptr >> 16;
		LOGMSG(env, "+++ mport_id: %u dest_id: %u\n", mport_id,
				*list_ptr & 0xffff);

		/* Display EPs for this MPORT */

		ret = riomp_mgmt_get_ep_list(mport_id, &ep_list,
				&number_of_eps);
		if (ret) {
			LOGMSG(env, "ERR: riodp_ep_get_list() ERR %d: %s\n",
					ret, strerror(ret));
			break;
		}

		printf("\t%u Endpoints (dest_ID): ", number_of_eps);
		for (ep = 0; ep < number_of_eps; ep++) {
			LOGMSG(env, "%u ", *(ep_list + ep));
		}
		LOGMSG(env, "\n");

		ret = riomp_mgmt_free_ep_list(&ep_list);
		if (ret) {
			LOGMSG(env, "ERR: riodp_ep_free_list() ERR %d: %s\n",
					ret, strerror(ret));
		}

	}

	LOGMSG(env, "\n");

	ret = riomp_mgmt_free_mport_list(&mport_list);
	if (ret) {
		LOGMSG(env, "ERR: riodp_ep_free_list() ERR %d: %s\n", ret,
				strerror(ret));
	}

exit:
	return 0;
}

struct cli_cmd Mpdevs = {
"mpdevs",
2,
0,
"Display mports and devices",
"<No Parameters>\n",
MpdevsCmd,
ATTR_NONE
};

static int UTimeCmd(struct cli_env *env, int argc, char **argv)
{
	uint16_t idx, st_i = 0, end_i = MAX_TIMESTAMPS-1;
	struct seq_ts *ts_p = NULL;
	uint64_t lim = 0;
	int got_one = 0;
	struct timespec diff, min, max, tot;

	if (gp_parse_worker_index_check_thread(env, argv[0], &idx, 0)) {
		goto exit;
	}

	switch (argv[1][0]) {
	case 'd':
	case 'D': 
		ts_p = &wkr[idx].desc_ts;
		break;
	case 'f':
	case 'F':
		ts_p = &wkr[idx].fifo_ts;
		break;
	case 'm':
	case 'M':
		ts_p = &wkr[idx].meas_ts;
		break;
	default:
		LOGMSG(env, "\nFAILED: <type> not 'd', 'f' or 'm'\n");
		goto exit;
	}
		
	switch (argv[2][0]) {
	case 's':
	case 'S':
		init_seq_ts(ts_p, MAX_TIMESTAMPS);
		break;
	case '-':
		if (argc > 4) {
			if (tok_parse_ushort(argv[3], &st_i, 0, MAX_TIMESTAMPS-1, 0)) {
				LOGMSG(env, "\n");
				LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "st_i", 0, MAX_TIMESTAMPS-1);
				goto exit;
			}
			if (tok_parse_ushort(argv[4], &end_i, 0, MAX_TIMESTAMPS-1, 0)) {
				LOGMSG(env, "\n");
				LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "end_i", 0, MAX_TIMESTAMPS-1);
				goto exit;
			}
		} else {
			LOGMSG(env, "\nFAILED: Must enter two indices\n");
			goto exit;
		}

		if (end_i < st_i) {
			LOGMSG(env, "\nFAILED: End index is less than start index\n");
			goto exit;
		}

		if (ts_p->ts_idx < MAX_TIMESTAMPS - 1) {
			LOGMSG(env, "\nWARNING: Last valid timestamp is %d\n",
					ts_p->ts_idx);
		}
		diff = time_difference(ts_p->ts_val[st_i], ts_p->ts_val[end_i]);
		LOGMSG(env, "\n---->> Sec<<---- Nsec---MMMuuuNNN\n");
		LOGMSG(env, "%16ld %16ld\n",
				diff.tv_sec, diff.tv_nsec);
		break;

	case 'p':
	case 'P':
		if ((argc > 3)&& (tok_parse_ushort(argv[3], &st_i, 0,
						MAX_TIMESTAMPS - 1, 0))) {
			LOGMSG(env, "\n");
			LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "st_i", 0,
					MAX_TIMESTAMPS-1);
			goto exit;
		}
		if ((argc > 4)&& (tok_parse_ushort(argv[4], &end_i, 0,
						MAX_TIMESTAMPS - 1, 0))) {
			LOGMSG(env, "\n");
			LOGMSG(env, TOK_ERR_USHORT_MSG_FMT, "end_i", 0,
					MAX_TIMESTAMPS-1);
			goto exit;
		}

		if (end_i < st_i) {
			LOGMSG(env, "\nFAILED: End index is less than start index\n");
			goto exit;
		}

		if (ts_p->ts_idx < MAX_TIMESTAMPS - 1) {
			LOGMSG(env,
				"\nWARNING: Last valid timestamp is %d\n",
				ts_p->ts_idx);
		}

		LOGMSG(env,
			"\nIdx ---->> Sec<<---- Nsec---mmmuuunnn Marker\n");
		for (idx = st_i; idx <= end_i; idx++) {
			LOGMSG(env, "%4d %16ld %16ld %d\n", idx,
				ts_p->ts_val[idx].tv_sec, 
				ts_p->ts_val[idx].tv_nsec,
				ts_p->ts_mkr[idx]);
		}
		break;

	case 'l':
	case 'L':
		if (argc > 3) {
			if (tok_parse_ull(argv[3], &lim, 0)) {
				LOGMSG(env, "\n");
				LOGMSG(env, TOK_ERR_ULL_HEX_MSG_FMT, "lim");
				goto exit;
			}
		} else {
			lim = 0;
		}

		for (idx = st_i; idx < end_i; idx++) {
			time_track(idx, ts_p->ts_val[idx], ts_p->ts_val[idx+1],
				&tot, &min, &max);
			diff = time_difference(ts_p->ts_val[idx],
						ts_p->ts_val[idx+1]);
			if ((uint64_t)diff.tv_nsec < lim)
				continue;
			if (!got_one) {
				LOGMSG(env,
				"\nIdx ---->> Sec<<---- Nsec---MMMuuuNNN Marker\n");
				got_one = 1;
			}
			LOGMSG(env, "%4d %16ld %16ld %d -> %d\n", idx,
				diff.tv_sec, diff.tv_nsec, 
				ts_p->ts_mkr[idx], ts_p->ts_mkr[idx+1]);
		}

		if (!got_one) {
			LOGMSG(env,
				"\nNo delays found bigger than %ld\n", lim);
		}
		LOGMSG(env,
			"\n==== ---->> Sec<<---- Nsec---MMMuuuNNN\n");
		LOGMSG(env, "Min: %16ld %16ld\n",
				min.tv_sec, min.tv_nsec);
		diff = time_div(tot, end_i - st_i);
		LOGMSG(env, "Avg: %16ld %16ld\n",
				diff.tv_sec, diff.tv_nsec);
		LOGMSG(env, "Max: %16ld %16ld\n",
				max.tv_sec, max.tv_nsec);
		break;
	default:
		LOGMSG(env, "FAILED: <cmd> not 's','p' or 'l'\n");
	}

exit:
	return 0;
}

struct cli_cmd UTime = {
"utime",
2,
3,
"Timestamp buffer command",
"<idx> <type> <cmd> <parms>\n"
	"<idx> is a worker index from 0 to " STR(MAX_WORKER_IDX) "\n"
	"<type> is:\n"
	"      'd' - descriptor timestamps\n"
	"      'f' - FIFO (descriptor completions)\n"
	"      'm' - measurement (development only)\n"
	"<cmd> is the command to perform on the buffer, one of:\n"
	"      's' - sample timestamps again\n"
	"      '-' - return difference in two timestamp idices\n"
	"            Note: Must enter two timestamp indexes\n"
	"      'p' - print the existing counter values\n"
	"            Note: optionally enter start and end indexes\n"
	"      'l' - locate differences greater than x nsec\n"
	"            Note: Must enter the number of nanoseconds\n",
UTimeCmd,
ATTR_NONE
};

struct cli_cmd *goodput_cmds[] = {
	&IBAlloc,
	&IBDealloc,
	&Dump,
	&Fill,
	&OBDIO,
	&OBDIOTxLat,
	&OBDIORxLat,
	&dmaTxLat,
	&dmaRxLat,
	&dma,
	&dmaNum,
	&msgTx,
	&msgRx,
	&msgTxLat,
	&msgRxLat,
	&msgTxOh,
	&msgRxOh,
	&Goodput,
	&Lat,
	&Status,
	&Thread,
	&Kill,
	&Halt,
	&Move,
	&Wait,
	&Sleep,
	&CPUOccSet,
	&CPUOccDisplay,
	&Mpdevs,
	&UTime,
};

void bind_goodput_cmds(void)
{
	dump_idx = 0;
	dump_base_offset = 0;
	dump_size = 0x100;
	cpu_occ_saved_idx = 0;

	cpu_occ_valid = 0;
	new_proc_user_jifis = 0;
	new_proc_kern_jifis = 0;
	old_proc_user_jifis = 0;
	old_proc_kern_jifis = 0;
	old_tot_jifis = 1;
	new_tot_jifis = 2;
	cpu_occ_pct = 0.0;

	add_commands_to_cmd_db(sizeof(goodput_cmds) / sizeof(goodput_cmds[0]),
			goodput_cmds);
}

#ifdef __cplusplus
}
#endif
