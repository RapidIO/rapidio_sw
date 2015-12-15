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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "librskt.h"
#include "librskt_private.h"
#include "librsktd_sn.h"
#include "libcli.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t max_skt;

enum rskt_state skts[RSKTD_NUM_SKTS];

void rsktd_sn_init(uint16_t max_skt_num)
{
	uint32_t i;

	max_skt = (uint32_t)(max_skt_num) + 1;
	for (i = 0; i < RSKTD_NUM_SKTS; i++)
		skts[i] = rskt_uninit;
};

enum rskt_state rsktd_sn_get(uint32_t skt_num)
{
	if (skt_num < max_skt)
		return skts[skt_num];

	return rskt_max_state;
};

void rsktd_sn_set(uint32_t skt_num, enum rskt_state st)
{
	if ((skt_num < max_skt) && (skt_num != RSKTD_INVALID_SKT))
		skts[skt_num] = st;
};

uint32_t rsktd_sn_find_free(uint32_t skt_num)
{
	uint32_t i;

	for (i = skt_num; i < max_skt; i++) {
		if ((rskt_uninit == skts[i]) || (rskt_closed == skts[i]))
			break;
	};
	if (i > max_skt)
		i = RSKTD_INVALID_SKT;
	return i;
};
	
int RSKTSniCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t max_skt_num = RSKTD_MAX_SKT_NUM;

	if (argc)
		max_skt_num = getDecParm(argv[0], RSKTD_MAX_SKT_NUM);

	sprintf(env->output, "max_skt_num = %d\n", max_skt_num);
	logMsg(env);
	rsktd_sn_init(max_skt_num);
	sprintf(env->output, "max_skt = %d\n", max_skt);
	logMsg(env);
	return 0;
};

struct cli_cmd RSKTSni = {
"rsktsni",
0,
0,
"RSKTD Socket Number Init Test Command.",
"{<max>}\n"
        "<max> Optional maximum socket number.\n"
	"Default value is 0xFFFF.\n",
RSKTSniCmd,
ATTR_NONE
};

uint32_t rsktd_sng_skt;

int RSKTSngCmd(struct cli_env *env, int argc, char **argv)
{
	enum rskt_state st;

	if (argc)
		rsktd_sng_skt = getDecParm(argv[0], RSKTD_MAX_SKT_NUM);
	else
		rsktd_sng_skt++;

	sprintf(env->output, "Socket = %d\n", rsktd_sng_skt);
	logMsg(env);
	st = rsktd_sn_get(rsktd_sng_skt);
	sprintf(env->output, "State = %d:\"%s\"\n", 
		(uint32_t)st, SKT_STATE_STR(st));
	logMsg(env);
	return 0;
};

struct cli_cmd RSKTSng = {
"rsktsng",
0,
0,
"RSKTD Socket Number State Get Test Command.",
"{<skt_num>}\n"
        "<skt_num> Optional socket number to query.\n",
RSKTSngCmd,
ATTR_RPT
};

uint32_t rsktd_sns_skt;
enum rskt_state rsktd_sns_st;

int RSKTSnsCmd(struct cli_env *env, int argc, char **argv)
{
	enum rskt_state st;

	if (argc) 
		rsktd_sns_skt = getDecParm(argv[0], RSKTD_MAX_SKT_NUM);
	else
		rsktd_sns_skt++;

	if (argc > 1)
		rsktd_sns_st = (enum rskt_state)getDecParm(argv[1], 
						(uint32_t)rskt_max_state);

	sprintf(env->output, "Socket = %d\n", rsktd_sns_skt);
	logMsg(env);
	st = rsktd_sn_get(rsktd_sns_skt);
	sprintf(env->output, "State B4 = %d:\"%s\"\n", 
		(uint32_t)st, SKT_STATE_STR(st));
	logMsg(env);
	rsktd_sn_set(rsktd_sns_skt, rsktd_sns_st);
	st = rsktd_sn_get(rsktd_sns_skt);
	sprintf(env->output, "State Aft= %d:\"%s\"\n", 
		(uint32_t)st, SKT_STATE_STR(st));
	logMsg(env);
	return 0;
};

struct cli_cmd RSKTSns = {
"rsktsns",
0,
0,
"RSKTD Socket Number State Set Test Command.",
"{<skt_num> <state>}\n"
        "<skt_num> Socket number to set.\n"
        "<state> State value to set.\n",
RSKTSnsCmd,
ATTR_RPT
};

int rsktd_snf_skt;

int RSKTDSnfCmd(struct cli_env *env, int argc, char **argv)
{
	int skt_num;
	enum rskt_state st;

	if (argc) 
		rsktd_snf_skt = getDecParm(argv[0], RSKTD_MAX_SKT_NUM);

	sprintf(env->output, "Socket = %d\n", rsktd_snf_skt);
	logMsg(env);
	skt_num = rsktd_sn_find_free(rsktd_snf_skt);
	st = rsktd_sn_get(skt_num);
	sprintf(env->output, "Free Skt= %d : %d:\"%s\"\n", skt_num,
		(int)st, SKT_STATE_STR(st));
	logMsg(env);

	rsktd_snf_skt = skt_num + 1;

	return 0;
};

struct cli_cmd RSKTSnf = {
"rsktsnf",
0,
0,
"RSKTD Socket Number Find Free Test Command.",
"{<skt_num>}\n"
        "<skt_num> Starting socket number.\n",
RSKTDSnfCmd,
ATTR_RPT
};

uint32_t rsktd_snd_skt;
uint32_t rsktd_snd_cnt;
enum rskt_state rsktd_snd_st;

int RSKTDSndCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t i;
	enum rskt_state st;

	if (argc) 
		rsktd_snd_skt = getDecParm(argv[0], RSKTD_MAX_SKT_NUM);

	if (argc > 1)
		rsktd_snd_cnt = getDecParm(argv[1], 100);

	if (argc > 2)
		rsktd_snd_st = (enum rskt_state)getDecParm(argv[2], 
				(uint32_t)rskt_max_state);

	rsktd_snd_skt -= (rsktd_snd_skt % 10);

	sprintf(env->output, "Index      0      1      2      3      4      5      6      7      8      9");
	logMsg(env);
	for (i = 0; i < rsktd_snd_cnt; i++) {
		int skt_num = i + rsktd_snd_skt;
		if (!(i % 10)) {
			sprintf(env->output, "\n%5d", skt_num);
			logMsg(env);
		};
		st = rsktd_sn_get(skt_num);
		if ((rsktd_snd_st == rskt_max_state) || (rsktd_snd_st == st))
			sprintf(env->output, "%7s", SKT_STATE_STR(st));
		else
			sprintf(env->output, "       ");
		logMsg(env);
	};
	sprintf(env->output, "\n");
	logMsg(env);

	rsktd_snd_skt += rsktd_snd_cnt;

	return 0;
};

struct cli_cmd RSKTSnd = {
"rsktsnd",
0,
0,
"RSKTD Socket State Dump Command.",
"{<skt_num> <num_skts>}\n"
        "<skt_num> Starting socket number.\n"
        "<num_skts> Number of sockets to display.\n",
RSKTDSndCmd,
ATTR_RPT
};

struct cli_cmd *sn_cmds[] = 
{ &RSKTSni,
&RSKTSng,
&RSKTSns,
&RSKTSnf,
&RSKTSnd
};

void librsktd_bind_sn_cli_cmds(void)
{
	rsktd_sng_skt = 0;
	rsktd_sns_skt = 0;
	rsktd_sns_st = rskt_uninit;
	rsktd_snf_skt = 0;
	rsktd_snd_skt = 0;
	rsktd_snd_cnt = 100;
	rsktd_snd_st = rskt_max_state;

        add_commands_to_cmd_db(sizeof(sn_cmds)/sizeof(sn_cmds[0]), 
				sn_cmds);

        return;
};
#ifdef __cplusplus
}
#endif
