/* Fabric Management Daemon Configuration file and options parsing support */
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
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>

#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "fmd_dd.h"
#include "fmd_cfg.h"
#include "libcli.h"
#include "liblog.h"

#ifdef __cplusplus
extern "C" {
#endif

void fmd_print_help(void)
{
	printf("\nThe RapidIO Fabric Management Daemon (\"FMD\") manages a\n");
	printf("RapidIO fabric defined in a configuration file.\n");
	printf("Options are:\n");
	printf("-a, -A<port>: POSIX Ethernet socket for App connections.\n");
	printf("       Default is %d\n", FMD_DFLT_APP_PORT_NUM);
	printf("-c, -C<filename>: FMD configuration file name.\n");
	printf("       Default is \"%s\"\n", FMD_DFLT_CFG_FN);
	printf("-d, -D<filename>: Device directory Posix SM file name.\n");
	printf("       Default is \"%s\"\n", FMD_DFLT_DD_FN);
	printf("-h, -H, -?: Print this message.\n");
	printf("-i<interval>: Interval between Device Directory updates.\n");
	printf("       Default is %d\n", FMD_DFLT_MAST_INTERVAL);
	printf("-l, -L<level>: Set starting logging level.\n");
	printf("       Default is %x\n", RDMA_LL);
	printf("-m, -M<filename>: Device directory Mutex SM file name.\n");
	printf("       Default is \"%s\"\n", FMD_DFLT_DD_MTX_FN);
	printf("-n, -N: Do not start console CLI.\n");
	printf("-p<port>: POSIX Ethernet socket for remote CLI.\n");
	printf("       Default is %d\n", FMD_DFLT_CLI_PORT_NUM);
	printf("-s, -S: Simple initialization, do not populate device dir.\n");
	printf("       Default is %d\n", FMD_DFLT_INIT_DD);
	printf("-x, -X: Initialize and then immediately exit.\n");
};

void update_string(char **value, char *parm, int len)
{
	if (NULL != *value)
		free(*value);
	*value = (char *)malloc(len+1);
	(*value)[len] = 0;
	memcpy(*value, parm, len);
};

int fmd_v_str(char **value, char *parm, int chk_slash)
{
	int len;

       	if (NULL == parm)
		return 1;
	len = strlen(parm);

	if (len < 2)
		return 1;
	if (chk_slash && ((parm[0] != '/') || (parm[1] == '/')))
		return 1;
	update_string(value, parm, len);
	return 0;
};

struct fmd_cfg_parms *fmd_parse_options(int argc, char *argv[])
{
	int idx, i, j;

	char *dflt_fmd_cfg = (char *)FMD_DFLT_CFG_FN;
	char *dflt_dd_fn = (char *)FMD_DFLT_DD_FN;
	char *dflt_dd_mtx_fn = (char *)FMD_DFLT_DD_MTX_FN;
	struct fmd_cfg_parms *cfg;

	cfg = (struct fmd_cfg_parms *)malloc(sizeof(struct fmd_cfg_parms));
	cfg->init_err = 0;
	cfg->init_and_quit = 0;
	cfg->simple_init = 0;
	cfg->print_help = 0;
	cfg->cli_port_num = FMD_DFLT_CLI_PORT_NUM;
	cfg->app_port_num = FMD_DFLT_APP_PORT_NUM;
	cfg->run_cons = 1;
	cfg->log_level = RDMA_LL;
	cfg->mast_idx = FMD_SLAVE;
	cfg->max_mport_info_idx = 0;
	for (i = 0; i < FMD_MAX_MPORTS; i++) {
		cfg->mport_info[i].num = -1;
		cfg->mport_info[i].mp_h = NULL;
		cfg->mport_info[i].ct = 0;
		cfg->mport_info[i].op_mode = -1;
		cfg->mport_info[i].mem_sz = CFG_MEM_SZ_DEFAULT;
		for (j = 0; j < CFG_DEVID_MAX; j++) {
			cfg->mport_info[i].devids[j].devid = 0;
			cfg->mport_info[i].devids[j].hc = 0xFF;
			cfg->mport_info[i].devids[j].valid = 0;
		};
		cfg->mport_info[i].ep = NULL;
		cfg->mport_info[i].ep_pnum = -1;
	};
	cfg->mast_devid_sz = FMD_DFLT_MAST_DEVID_SZ;
	cfg->mast_devid = FMD_DFLT_MAST_DEVID;
	cfg->mast_cm_port = FMD_DFLT_MAST_CM_PORT;
	cfg->mast_interval = FMD_DFLT_MAST_INTERVAL;
	update_string(&cfg->fmd_cfg, dflt_fmd_cfg, strlen(dflt_fmd_cfg));
	update_string(&cfg->dd_fn, dflt_dd_fn, strlen(dflt_dd_fn));
	update_string(&cfg->dd_mtx_fn, dflt_dd_mtx_fn, strlen(dflt_dd_mtx_fn));

	for (i = 0; i < FMD_MAX_EP; i++) {
		cfg->eps[i].valid = 0;
		cfg->eps[i].ep_h = NULL;
		cfg->eps[i].name = NULL;
		cfg->eps[i].port_cnt = 0;
		for (j = 0; j < FMD_MAX_EP_PORT; j++) {
			int k;
			cfg->eps[i].ports[j].valid = 0;
			cfg->eps[i].ports[j].port = 0;
			cfg->eps[i].ports[j].ct = 0;
			cfg->eps[i].ports[j].rio.max_pw = idt_pc_pw_last;
			cfg->eps[i].ports[j].rio.op_pw = idt_pc_pw_last;
			cfg->eps[i].ports[j].rio.ls = idt_pc_ls_last;
			cfg->eps[i].ports[j].rio.idle2 = 0;
			cfg->eps[i].ports[j].rio.em = 0;
			for (k = 0; k < FMD_DEVID_MAX; k++) {
				cfg->eps[i].ports[j].devids[k].hc = 0xff;
				cfg->eps[i].ports[j].devids[k].devid = 0;
				cfg->eps[i].ports[j].devids[k].valid = 0;
			}
			cfg->eps[i].ports[j].conn = NULL;
			cfg->eps[i].ports[j].conn_end = -1;
		};
	};

	for (i = 0; i < FMD_MAX_SW; i++) {
		cfg->sws[i].valid = 0;
		cfg->sws[i].sw_h = NULL;
		cfg->sws[i].name = NULL;
		cfg->sws[i].dev_type = NULL;
		cfg->sws[i].did_sz = 0;
		cfg->sws[i].did = 0;
		cfg->sws[i].hc = 0;
		cfg->sws[i].ct = 0;
		cfg->sws[i].traversed = 0;
		for (j = 0; j < FMD_MAX_SW_PORT; j++) {
			cfg->sws[i].ports[j].valid = 0;
			cfg->sws[i].ports[j].rio.max_pw = idt_pc_pw_last;
			cfg->sws[i].ports[j].rio.op_pw = idt_pc_pw_last;
			cfg->sws[i].ports[j].rio.ls = idt_pc_ls_last;
			cfg->sws[i].ports[j].rio.idle2 = 0;
			cfg->sws[i].ports[j].rio.em = 0;
			cfg->sws[i].ports[j].conn = NULL;
			cfg->sws[i].ports[j].conn_end = -1;
		};
		for (j = 0; j < FMD_DEVID_MAX; j++) {
			int k;
			cfg->sws[i].rt[j].default_route = 0xDF;
			for (k = 0; k < IDT_DAR_RT_DEV_TABLE_SIZE; k++) {
				cfg->sws[i].rt[j].dev_table[k].rte_val =
							IDT_DSF_RT_NO_ROUTE;
				cfg->sws[i].rt[j].dev_table[k].changed = 0;
				cfg->sws[i].rt[j].dom_table[k].rte_val =
							IDT_DSF_RT_NO_ROUTE;
				cfg->sws[i].rt[j].dom_table[k].changed = 0;
			};
			for (k = 0; k < IDT_DSF_MAX_MC_MASK; k++) {
				cfg->sws[i].rt[j].mc_masks[k].mc_destID = 0xFF;
				cfg->sws[i].rt[j].mc_masks[k].tt = tt_dev8;
				cfg->sws[i].rt[j].mc_masks[k].mc_mask = 0;
				cfg->sws[i].rt[j].mc_masks[k].in_use = 0;
				cfg->sws[i].rt[j].mc_masks[k].allocd = 0;
				cfg->sws[i].rt[j].mc_masks[k].changed = 0;
			};
		};
	};

	for (i = 0; i < FMD_MAX_CONN; i++) {
		int e;
		cfg->cons[i].valid = 0;
		for (e = 0; e < 2; e++) {
			cfg->cons[i].ends[e].port_num = -1;
			cfg->cons[i].ends[e].ep = -1;
			cfg->cons[i].ends[e].sw_h = NULL;
		};
	};

	for (idx = 0; idx < argc; idx++) {
		if (strnlen(argv[idx], 4) < 2)
			continue;

		if ('-' == argv[idx][0]) {
			switch(argv[idx][1]) {
			case 'a': 
			case 'A': cfg->app_port_num= atoi(&argv[idx][2]);
				  break;
			case 'c': 
			case 'C': if (fmd_v_str(&cfg->fmd_cfg, 
							  &argv[idx][2], 0))
					  goto print_help;
				  break;
			case 'd': 
			case 'D': if (fmd_v_str(&cfg->dd_fn,
							  &argv[idx][2], 1))
					  goto print_help;
				  break;
			case '?': 
			case 'h': 
			case 'H': goto print_help;

			case 'i': 
			case 'I': cfg->mast_interval = atoi(&argv[idx][2]);
				  break;
			case 'l': 
			case 'L': cfg->log_level = atoi(&argv[idx][2]);
				  break;
			case 'm': 
			case 'M': if (fmd_v_str(&cfg->dd_mtx_fn,
							&argv[idx][2], 1))
					  goto print_help;
				  break;
			case 'n': 
			case 'N': cfg->run_cons = 0;
				  break;

			case 'p': 
			case 'P': cfg->cli_port_num= atoi(&argv[idx][2]);
				  break;
			case 's': 
			case 'S': cfg->simple_init = 1;
				  break;
			case 'x': 
			case 'X': cfg->init_and_quit = 1;
				  break;
			default: printf("\nUnknown parm: \"%s\"\n", argv[idx]);
				 goto print_help;
			};
		};
	}
	return cfg;

print_help:
	cfg->init_and_quit = 1;
	cfg->print_help = 1;
	fmd_print_help();
	return cfg;
}

void strip_crlf(char *tok)
{
	char *temp;

	if (NULL == tok)
		return;
	temp = strchr(tok, '\n');
	if (NULL != temp)
		temp[0] = '\0';
	temp = strchr(tok, '\r');
	if(NULL != temp)
		temp[0] = '\0';
};

const char *delim = " 	";

void flush_comment(char *tok)
{
	while (NULL != tok) {
		DBG("%s\n", tok);
		tok = strtok(NULL, delim);
		strip_crlf(tok);
	};
};

#define LINE_SIZE 256
char *line;

char *try_get_next_token(struct fmd_cfg_parms *cfg)
{
	char *rc = NULL;
	size_t byte_cnt = 1;
	int done = 0;

	if (cfg->init_err)
		goto fail;

	if (NULL == line) {
		line = (char *)malloc(LINE_SIZE);
		rc = NULL;
	} else {
		rc = strtok(NULL, delim);
	};

	while (!done) {
		while ((NULL == rc) && (byte_cnt > 0)) {
			DBG("\n");
			byte_cnt = LINE_SIZE;
			byte_cnt = getline(&line, &byte_cnt, cfg->fmd_cfg_fd);
			strip_crlf(line);
			rc = strtok(line, delim);
		};

		if (byte_cnt <= 0) {
			rc = NULL;
			break;
		};

		if (NULL != rc) {
			done = strncmp(rc, "//", 2);
			if (!done) {
				flush_comment(rc);
				rc = NULL;
			}
		}
	};

	if (NULL != rc)
		DBG("%s\n", rc);

fail:
	return rc;
};

void parse_err(struct fmd_cfg_parms *cfg, char *err_msg)
{
	ERR("\n%s\n", err_msg);
	cfg->init_err = 1;
};

int get_next_token(struct fmd_cfg_parms *cfg, char **token)
{
	if (cfg->init_err) {
		*token = NULL;
		return 1;
	};

	*token = try_get_next_token(cfg);

       	if (NULL == *token) 
		parse_err(cfg, (char *)"Unexpected end of file.");

	return (NULL == *token);
};

int parm_idx(char *token, char *token_list)
{
	int rc = 0;
	int len;
       
	if (NULL == token)
		return -1;

	len = strlen(token);

	while (NULL != token_list) {
		if (!strncmp(token, token_list, len))
			if ((' ' == token_list[len]) || (0 == token_list[len]))
				break;
		rc++;
		token_list = strchr(token_list, ' ');
		if (NULL != token_list)
			token_list = &token_list[1];
	}
	return rc;
};

#define DEVID_SZ_TOKENS "dev08 dev16 dev32"
#define DEVID_SZ_TOKENS_END "dev08 dev16 dev32 END"

int get_devid_sz(struct fmd_cfg_parms *cfg, uint32_t *devID_sz)
{
	char *tok;

	if (cfg->init_err)
		goto fail;
	if (get_next_token(cfg, &tok))
		goto fail;

	switch (parm_idx(tok, (char *)DEVID_SZ_TOKENS)) {
	case 0: // "dev08"
		*devID_sz |= FMD_DEV08;
		break;
	case 1: // "dev16"
		*devID_sz |= FMD_DEV16;
		break;
	case 2: // "dev32"
		*devID_sz |= FMD_DEV32;
		break;
	default:
		parse_err(cfg, (char *)"Unknown devID size.");
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int get_dec_int(struct fmd_cfg_parms *cfg, uint32_t *dec_int)
{
	char *tok;

	if (cfg->init_err || get_next_token(cfg, &tok))
		goto fail;
	*dec_int = atoi(tok);
	return 0;
fail:
	return 1;
};

int get_hex_int(struct fmd_cfg_parms *cfg, uint32_t *hex_int)
{
	char *tok, *endptr;

	if (cfg->init_err || get_next_token(cfg, &tok))
		goto fail;
	errno = 0;
	*hex_int = strtol(tok, &endptr, 16);

	if ((errno == ERANGE && (*hex_int == 0xFFFFFFFF || *hex_int == 0))
		|| ((errno != 0) && (*hex_int == 0)))
		goto fail;

	if (endptr == tok)
		goto fail;

	return 0;
fail:
	return 1;
};

int get_port_num(struct fmd_cfg_parms *cfg, uint32_t *pnum)
{
	if (get_dec_int(cfg, pnum))
		return 1;

	if (*pnum > 17) {
		parse_err(cfg, (char *)"Illegal portnum.");
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int get_parm_idx(struct fmd_cfg_parms *cfg, char *parm_list)
{
	char *tok;

	if (!get_next_token(cfg, &tok))
		return parm_idx(tok, parm_list);
	return -1;
};

int get_string(struct fmd_cfg_parms *cfg, char **parm)
{
	char *tok;

	if (!get_next_token(cfg, &tok)) {
		update_string(parm, tok, strlen(tok));
		return 0;
	}
	return 1;
};

int get_rt_v(struct fmd_cfg_parms *cfg, uint32_t *rt_val)
{
	char *tok;

	if (get_next_token(cfg, &tok))
		goto fail;
	switch (parm_idx(tok, (char *)"MC NEXT_BYTE DEFAULT DROP")) {
	case 0: // MC
		if (get_dec_int(cfg, rt_val))
			goto fail;
		if (*rt_val >= IDT_DSF_MAX_MC_MASK) {
			parse_err(cfg, (char *)"Illegal MC Mask number.");
			goto fail;
		};
		*rt_val = *rt_val + IDT_DSF_FIRST_MC_MASK;
		break;
	case 1: // NEXT_BYTE
		*rt_val = IDT_DSF_RT_USE_DEVICE_TABLE;
		break;
	case 2: // DEFAULT
		*rt_val = IDT_DSF_RT_USE_DEFAULT_ROUTE;
		break;
	case 3: // DROP
		*rt_val = IDT_DSF_RT_NO_ROUTE;
		break;
	default:
		*rt_val = atoi(tok);
		if (*rt_val >= IDT_DSF_FIRST_MC_MASK) {
			parse_err(cfg, (char *)"Illegal port number.");
			goto fail;
		};
	};
	return 0;
fail:
	return 1;
};

int find_ep_name(struct fmd_cfg_parms *cfg, char *name, struct fmd_cfg_ep **ep)
{
	uint32_t i;

	for (i = 0; i < cfg->ep_cnt; i++) {
		if (!strcmp(name, cfg->eps[i].name) && 
			(strlen(name) == strlen(cfg->eps[i].name))) {
			*ep = &cfg->eps[i];
			return 0;
		};
	};
	*ep = NULL;
	return 1;
};

int find_sw_name(struct fmd_cfg_parms *cfg, char *name, struct fmd_cfg_sw **sw)
{
	uint32_t i;

	for (i = 0; i < cfg->sw_cnt; i++) {
		if (!strcmp(name, cfg->sws[i].name) && 
			(strlen(name) == strlen(cfg->sws[i].name))) {
			*sw = &cfg->sws[i];
			return 0;
		};
	};
	*sw = NULL;
	return 1;
};

int find_ep_and_port(struct fmd_cfg_parms *cfg, char *tok, 
			struct fmd_cfg_ep **ep, int *port)
{
	char *temp;

	*port = 0;
	*ep = NULL;

	temp = strchr(tok, '.');
	if (NULL == temp)
		return 1;

	if ('.' == temp[0]) {
		temp[0] = '\0';
		*port = atoi(&temp[1]);
		if ((*port < 0) || (*port >= FMD_MAX_EP_PORT)) {
			parse_err(cfg, (char *)"Illegal port index.");
			goto fail;
		};
	};

	if (find_ep_name(cfg, tok, ep))
		goto fail;

	if (!(*ep)->ports[*port].valid) {
		parse_err(cfg, (char *)"Invalid port selected.");
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int find_sw_and_port(struct fmd_cfg_parms *cfg, char *tok, 
			struct fmd_cfg_sw **sw, int *port)
{
	char *temp;

	*port = 0;
	*sw = NULL;

	temp = strchr(tok, '.');
	if ('.' == temp[0]) {
		temp[0] = '\0';
		*port = atoi(&temp[1]);
		if ((*port < 0) || (*port >= FMD_MAX_EP_PORT)) {
			parse_err(cfg, (char *)"Illegal port index.");
			goto fail;
		};
	};

	if (find_sw_name(cfg, tok, sw))
		goto fail;

	if (!(*sw)->ports[*port].valid) {
		parse_err(cfg, (char *)"Invalid port selected.");
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int get_ep_sw_and_port(struct fmd_cfg_parms *cfg, struct fmd_cfg_conn *conn, 
			int idx)
{
	char *temp, *tok;

	conn->ends[idx].port_num = 0;
	conn->ends[idx].ep = 1;
	conn->ends[idx].ep_h = NULL;

	if (get_next_token(cfg, &tok))
		goto fail;

	temp = strchr(tok, '.');
	if ('.' == temp[0]) {
		temp[0] = '\0';
		conn->ends[idx].port_num = atoi(&temp[1]);
		if (conn->ends[idx].port_num < 0) {
			parse_err(cfg, (char *)"Illegal port index.");
			goto fail;
		};
	};

	if (!find_ep_name(cfg, tok, &conn->ends[idx].ep_h)) {
		if (conn->ends[idx].port_num >= FMD_MAX_EP_PORT) {
			parse_err(cfg, (char *)"Illegal port index.");
			goto fail;
		};
		if (!conn->ends[idx].ep_h->ports[conn->ends[idx].port_num].valid) {
			parse_err(cfg, (char *)"Invalid port selected.");
			goto fail;
		};
		conn->ends[idx].ep_h->ports[conn->ends[idx].port_num].conn
			= conn;
		conn->ends[idx].ep_h->ports[conn->ends[idx].port_num].conn_end
			= idx;
		return 0;
	};

	if (!find_sw_name(cfg, tok, &conn->ends[idx].sw_h)) {
		if (conn->ends[idx].port_num >= FMD_MAX_SW_PORT) {
			parse_err(cfg, (char *)"Illegal port index.");
			goto fail;
		};
		if (!conn->ends[idx].sw_h->ports[conn->ends[idx].port_num].valid) {
			parse_err(cfg, (char *)"Invalid port selected.");
			goto fail;
		};
		conn->ends[idx].sw_h->ports[conn->ends[idx].port_num].conn 
			= conn;
		conn->ends[idx].sw_h->ports[conn->ends[idx].port_num].conn_end 
			= idx;
		return 0;
	};
fail:
	return 1;
};

int get_destid(struct fmd_cfg_parms *cfg, uint32_t *destid, uint32_t devid_sz)
{
	int port = 0;
	char *tok;
	struct fmd_cfg_ep *ep;

	if (get_next_token(cfg, &tok))
		goto fail;

	if (find_ep_and_port(cfg, tok, &ep, &port)) {
		if (cfg->init_err)
			goto fail;
		*destid = atoi(tok);
		return 0;
	};

	if (!ep->ports[port].devids[devid_sz].valid) {
		parse_err(cfg, (char *)"Unconfigured devid selected.");
		goto fail;
	};
	*destid = ep->ports[port].devids[devid_sz].devid;
	return 0;
fail:
	return 1;
};

int parse_devid_sizes(struct fmd_cfg_parms *cfg, int *dev_id_szs)
{
	int done = 0, devid_sz;

	while (!done) {
		devid_sz = get_parm_idx(cfg, (char *)DEVID_SZ_TOKENS_END);
		switch (devid_sz) {
			case 0: // dev08
				*dev_id_szs |= FMD_DEV08;
				break;
			case 1: // dev16
				*dev_id_szs |= FMD_DEV16;
				break;
			case 2: // dev32
				*dev_id_szs |= FMD_DEV32;
				break;
			case 3: // END
				done = 1;
				break;
			default:
				goto fail;
		};
	};
	return 0;
fail:
	return 1;
};

#define MEM_SZ_TOKENS "mem34 mem50 mem66"

int parse_mport_mem_size(struct int_cfg_parms *cfg, uint8_t *mem_sz)
{
	uint32_t idx;

	idx = get_parm_idx(cfg, (char *)MEM_SZ_TOKENS);
	switch (idx) {
	case 0: // mem34
		*mem_sz = CFG_MEM_SZ_34;
		break;
	case 1: // mem50
		*mem_sz = CFG_MEM_SZ_50;
		break;
	case 2: // mem66
		*mem_sz = CFG_MEM_SZ_66;
		break;
	default: // Unknown
		goto fail;
	}
	return 0;
fail:
	return 1;
};

int parse_ep_devids(struct int_cfg_parms *cfg, struct dev_id *devids)
{
	uint32_t devid_sz, done = 0;

	for (devid_sz = 0; devid_sz < 3; devid_sz++) {
		devids[devid_sz].valid = 0;
		devids[devid_sz].hc = 255;
		devids[devid_sz].devid = 0;
	};

	while (!done) {
		devid_sz = get_parm_idx(cfg, (char *)DEVID_SZ_TOKENS_END);
		switch (devid_sz) {
			case 0: // dev08
			case 1: // dev16
			case 2: // dev32
				if (get_dec_int(cfg, &devids[devid_sz].devid))
					goto fail;
				if (get_dec_int(cfg, &devids[devid_sz].hc))
					goto fail;
				devids[devid_sz].valid = 1;
				break;
			case 3: // END
				done = 1;
				break;
			default:
				goto fail;
		};
	};
	return 0;
fail:
	return 1;
};

int check_match (struct dev_id *mp_did, struct dev_id *ep_did,
		struct fmd_mport_info *mpi, struct fmd_cfg_parms *cfg, 
		struct fmd_cfg_ep *ep, int pnum)
{
	if (!mp_did->valid)
		goto exit;
	if (!ep_did->valid)
		goto exit;
	if (mp_did->devid != ep_did->devid)
		goto exit;
	if (mp_did->hc != ep_did->hc)
		goto exit;
	if (mpi->ep != NULL) {
		parse_err(cfg, (char *)"Duplicate MPORT definitions");
		goto fail;
	};
	mpi->ep = ep;
	mpi->ep_pnum = pnum;

exit:
	return 0;
fail:
	return -1;
};

int match_ep_to_mports(struct fmd_cfg_parms *cfg, struct fmd_cfg_ep_port *ep_p,
			int pt_i, struct fmd_cfg_ep *ep)
{
	uint32_t mp_i, did_sz;
	struct dev_id *mp_did;
	struct dev_id *ep_did = ep_p->devids;

	for (mp_i = 0; mp_i < cfg->max_mport_info_idx; mp_i++) {
		mp_did = cfg->mport_info[mp_i].devids;
		for (did_sz = 0; did_sz < FMD_DEVID_MAX; did_sz++) {
			if (check_match( &mp_did[did_sz], &ep_did[did_sz],
					&cfg->mport_info[mp_i], cfg, ep, pt_i))
				return -1;
		};
	};
	return 0;
};

int parse_mport_info(struct fmd_cfg_parms *cfg)
{
	int idx, i;

	if (cfg->max_mport_info_idx >= FMD_MAX_MPORTS) {
		parse_err(cfg, (char *)"Too many MPORTs.");
		goto fail;
	};

	idx = cfg->max_mport_info_idx;
	if (get_dec_int(cfg, &cfg->mport_info[idx].num))
		goto fail;

	cfg->max_mport_info_idx++;

	for (i = 0; i < idx; i++) {
		if (cfg->mport_info[i].num == cfg->mport_info[idx].num) {
			parse_err(cfg, (char *)"Duplicate mport number.");
			goto fail;
		};
	};

	switch (get_parm_idx(cfg, (char *)"master slave")) {
	case 0: // "master"
		if (FMD_SLAVE != cfg->mast_idx) {
			parse_err(cfg, 
			(char *)"Only one MPORT can be master for now.");
			goto fail;
		};
		cfg->mport_info[idx].op_mode = FMD_OP_MODE_MASTER;
		cfg->mast_idx = idx;
		break;
	case 1: // "slave"
		cfg->mport_info[idx].op_mode = FMD_OP_MODE_SLAVE;
		break;
	default:
		parse_err(cfg, (char *)"Unknown operating mode.");
		goto fail;
	};

	if (parse_mport_mem_size(cfg, &cfg->mport_info[idx].mem_sz))
		goto fail;

	return parse_ep_devids(cfg, cfg->mport_info[idx].devids);
fail:
	return 1;
};

int parse_master_info(struct fmd_cfg_parms *cfg)
{
	if (get_devid_sz(cfg, &cfg->mast_devid_sz))
		goto fail;

	if (get_dec_int(cfg, &cfg->mast_devid))
		goto fail;

	if (get_dec_int(cfg, &cfg->mast_cm_port))
		goto fail;

	return 0;
fail:
	return 1;
};

int parse_mc_mask(struct fmd_cfg_parms *cfg, idt_rt_mc_info_t *mc_info)
{
	uint32_t mc_mask_idx, done = 0, pnum;
	char *tok;

	if (get_dec_int(cfg, &mc_mask_idx))
		goto fail;
	if (mc_mask_idx >= IDT_DSF_MAX_MC_MASK) {
		parse_err(cfg, (char *)"Illegal multicast mask index.");
		goto fail;
	};

	mc_info[mc_mask_idx].mc_destID = 0;
	mc_info[mc_mask_idx].tt = tt_dev8;
	mc_info[mc_mask_idx].mc_mask = 0;

	while (!done) {
		if (get_next_token(cfg, &tok))
			goto fail;
		switch (parm_idx(tok, (char *)"END")) {
		case 0: // END
			done = 1;
			break;
		default: 
			pnum = atoi(tok);
			if ((pnum < 0) || (pnum >= FMD_MAX_SW_PORT)) {
				parse_err(cfg, 
					(char *)"Illegal multicast port.");
				goto fail;
			};
			mc_info[mc_mask_idx].mc_mask |= (1 << pnum);
			break;
		};
	};
	mc_info[mc_mask_idx].in_use = 1;
	mc_info[mc_mask_idx].allocd = 1;
	mc_info[mc_mask_idx].changed = 1;
	return 0;
fail:
	return 1;
};

int get_port_width(struct fmd_cfg_parms *cfg, idt_pc_pw_t *pw);
int get_lane_speed(struct fmd_cfg_parms *cfg, idt_pc_ls_t *ls);
int get_idle_seq(struct fmd_cfg_parms *cfg, int *idle);

int parse_rapidio(struct fmd_cfg_parms *cfg, struct fmd_cfg_rapidio *rio)
{
	if (get_port_width(cfg, &rio->max_pw))
		goto fail;
	if (get_port_width(cfg, &rio->op_pw))
		goto fail;
	if (get_lane_speed(cfg, &rio->ls))
		goto fail;
	if (get_idle_seq(cfg, &rio->idle2))
		goto fail;

	switch (get_parm_idx(cfg, (char *)"EM_OFF EM_ON")) {
	case 0: // "OFF" 
		rio->em = 0;
		break;
	case 1: // "OFF" 
		rio->em = 1;
		break;
	default:
		parse_err(cfg, (char *)"Unknown error management config.");
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int parse_ep_port(struct fmd_cfg_parms *cfg, struct fmd_cfg_ep_port *prt)
{

	if (get_dec_int(cfg, &prt->port))
		goto fail;
	if (get_hex_int(cfg, &prt->ct))
		goto fail;
	if (parse_rapidio(cfg, &prt->rio))
		goto fail;
	if (parse_ep_devids(cfg, prt->devids))
		goto fail;
	prt->valid = 1;
	return 0;
fail:
	return 1;
};

int parse_endpoint(struct fmd_cfg_parms *cfg)
{
	int i = cfg->ep_cnt;
	int done = 0;

	if (i >= FMD_MAX_EP) {
		parse_err(cfg, (char *)"Too many endpoints.");
		goto fail;
	};

	if (get_string(cfg, &cfg->eps[i].name))
		goto fail;

	cfg->eps[i].port_cnt = 0;
	while (!done && (cfg->eps[i].port_cnt < FMD_MAX_EP_PORT + 1)) {
		int pt_i;
		switch (get_parm_idx(cfg, (char *)"PORT PEND")) {
		case 0: // "PORT"
	       		pt_i = cfg->eps[i].port_cnt;
			if (cfg->eps[i].port_cnt >= FMD_MAX_EP_PORT) {
				parse_err(cfg, (char *)"Too many ports!");
				goto fail;
			};
			if (parse_ep_port(cfg, &cfg->eps[i].ports[pt_i]))
				goto fail;
			cfg->eps[i].port_cnt++;
			if (match_ep_to_mports(cfg, &cfg->eps[i].ports[pt_i], 
						pt_i, &cfg->eps[i]))
				goto fail;
			break;
		case 1: // "PEND"
			done = 1;
			break;
		default:
			parse_err(cfg, (char *)"Unknown parameter.");
			goto fail;
		};
	};
				
	cfg->eps[i].valid = 1;
	cfg->ep_cnt++;
	return 0;
fail:
	return 1;
};

int assign_rt_v(int rt_sz, int st_destid, int end_destid, int rtv, 
			idt_rt_state_t *rt, struct fmd_cfg_parms *cfg)
{
	int i;

	switch (rt_sz) {
	case 0: // dev08
		if ((st_destid >= IDT_DAR_RT_DEV_TABLE_SIZE) ||
			(end_destid >= IDT_DAR_RT_DEV_TABLE_SIZE)) {
			parse_err(cfg, (char *)"DestID value too large.");
			goto fail;
		};
		if (st_destid > end_destid) {
			int temp = end_destid;
			end_destid = st_destid;
			st_destid = temp;
		};
		for (i = st_destid; i <= end_destid; i++) {
			if (rt->dev_table[i].rte_val != rtv) {
				rt->dev_table[i].rte_val = rtv;
				rt->dev_table[i].changed = 1;
			};
		};
		break;
	case 1: // dev16
		parse_err(cfg, (char *)"Dev16 not supported yet.");
		goto fail;
	case 2: // dev32
		parse_err(cfg, (char *)"Dev32 not supported yet.");
		goto fail;
	default:
		parse_err(cfg, (char *)"Unknown rt size.");
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int get_lane_speed(struct fmd_cfg_parms *cfg, idt_pc_ls_t *ls)
{
	switch (get_parm_idx(cfg, (char *)"1p25 2p5 3p125 5p0 6p25")) {
	case 0: // 1p25
		*ls = idt_pc_ls_1p25;
		break;
	case 1: // 2p5
		*ls = idt_pc_ls_2p5;
		break;
	case 2: // 3p125
		*ls = idt_pc_ls_3p125;
		break;
	case 3: // 5p0
		*ls = idt_pc_ls_5p0;
		break;
	case 4: // 6p25
		*ls = idt_pc_ls_6p25;
		break;
	default:
		parse_err(cfg, (char *)"Unknown lane speed.");
		goto fail;
	};

	return 0;
fail:
	return 1;
};

int get_port_width(struct fmd_cfg_parms *cfg, idt_pc_pw_t *pw)
{
	switch (get_parm_idx(cfg, (char *)"1x 2x 4x 1x_l0 1x_l1 1x_l2")) {
	case 0: // 1x
		*pw = idt_pc_pw_1x;
		break;
	case 1: // 2x
		*pw = idt_pc_pw_2x;
		break;
	case 2: // 4x
		*pw = idt_pc_pw_4x;
		break;
	case 3: // 1x lane 0
		*pw = idt_pc_pw_1x_l0;
		break;
	case 4: // 1x lane 1
		*pw = idt_pc_pw_1x_l1;
		break;
	case 5: // 1x lane 2
		*pw = idt_pc_pw_1x_l2;
		break;
	default:
		parse_err(cfg, (char *)"Unknown port width.");
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int get_idle_seq(struct fmd_cfg_parms *cfg, int *idle)
{
	*idle = get_parm_idx(cfg, (char *)"IDLE1 IDLE2");
	if ((*idle < 0) || (*idle > 1)) {
		parse_err(cfg, (char *)"Unknown idle sequence.");
		goto fail;
	};
	return 0;
fail:
	return 1;
};

int parse_sw_port(struct fmd_cfg_parms *cfg)
{
	uint32_t idx = cfg->sw_cnt;
	uint32_t port;

	if (get_dec_int(cfg, &port))
		goto fail;

	if (parse_rapidio(cfg, &cfg->sws[idx].ports[port].rio))
		goto fail;

	cfg->sws[idx].ports[port].valid = 1;
	cfg->sws[idx].ports[port].port  = port;

	return 0;
fail:
	return 1;
};

int parse_switch(struct fmd_cfg_parms *cfg)
{
	uint32_t i, done = 0;
	uint32_t rt_sz = 0;
	uint32_t destid, destid1;
	uint32_t rtv;

	if (cfg->sw_cnt >= FMD_MAX_SW) {
		parse_err(cfg, (char *)"Too many switches.");
		goto fail;
	};
	i = cfg->sw_cnt;

	if (get_string(cfg, &cfg->sws[i].dev_type))
		goto fail;
	if (get_string(cfg, &cfg->sws[i].name))
		goto fail;
	if (get_devid_sz(cfg, &cfg->sws[i].did_sz))
		goto fail;
	if (get_dec_int(cfg, &cfg->sws[i].did))
		goto fail;
	if (get_dec_int(cfg, &cfg->sws[i].hc))
		goto fail;
	if (get_dec_int(cfg, &cfg->sws[i].ct))
		goto fail;

	while (!done) {
		switch(get_parm_idx(cfg, (char *)
		"PORT ROUTING_TABLE DFLTPORT DESTID RANGE MCMASK END")) {
		case 0: // PORT
			if (parse_sw_port(cfg))
				goto fail;
			break;
		case 1: // ROUTING_TABLE
			rt_sz = get_parm_idx(cfg, (char *)DEVID_SZ_TOKENS);
			if ((rt_sz < 0) || (rt_sz > 2)) {
				parse_err(cfg, (char *)"Unknown devID size.");
				goto fail;
			};
			break;
		case 2: // DFLTPORT
			if (get_rt_v(cfg, &rtv))
				goto fail;
			cfg->sws[i].rt[rt_sz].default_route = rtv;
			break;
		case 3: // DESTID
			if (get_destid(cfg, &destid, rt_sz))
				goto fail;
			if (get_rt_v(cfg, &rtv))
				goto fail;
			if (assign_rt_v(rt_sz, destid, destid, rtv, 
					&cfg->sws[i].rt[rt_sz], cfg)) {
				parse_err(cfg, (char *)"Illegal destID/rtv.");
				goto fail;
			};
			break;
		case 4: // RANGE
			if (get_destid(cfg, &destid, rt_sz))
				goto fail;
			if (get_destid(cfg, &destid1, rt_sz))
				goto fail;
			if (get_rt_v(cfg, &rtv))
				goto fail;
			if (assign_rt_v(rt_sz, destid, destid1, rtv, 
					&cfg->sws[i].rt[rt_sz], cfg)) {
				parse_err(cfg, (char *)"Illegal destID/rtv.");
				goto fail;
			};
			break;
		case 5: // MCMASK
			if (parse_mc_mask(cfg, cfg->sws[i].rt[rt_sz].mc_masks))
				goto fail;
			break;
		case 6: // END
			done = 1;
			break;
		default:
			parse_err(cfg, (char *)"Unknown parameter.");
			goto fail;
		};
	};

	cfg->sw_cnt++;
	return 0;
fail:
	return 1;
};

int parse_connect(struct fmd_cfg_parms *cfg)
{
	int idx = cfg->conn_cnt;

	if (cfg->conn_cnt >= FMD_MAX_CONN) {
		parse_err(cfg, (char *)"Too many connections.");
		goto fail;
	};
	if (get_ep_sw_and_port(cfg, &cfg->cons[idx], 0))
		goto fail;
	if (get_ep_sw_and_port(cfg, &cfg->cons[idx], 1))
		goto fail;
	cfg->conn_cnt++;
	cfg->cons[idx].valid = 1;

	return 0;
fail:
	return 1;
};

void fmd_parse_cfg(struct fmd_cfg_parms *cfg)
{
	char *tok;
	// size_t byte_cnt = LINE_SIZE;

	tok = try_get_next_token(cfg);

	while ((NULL != tok) && !cfg->init_err) {
		switch (parm_idx(tok, (char *)
	"// DEV_DIR DEV_DIR_MTX MPORT MASTER_INFO ENDPOINT SWITCH CONNECT EOF")) {
		case 0: // "//"
			flush_comment(tok);
			break;
		case 1: // "DEV_DIR"
			if (get_next_token(cfg, &tok))
				break;
			if (fmd_v_str(&cfg->dd_fn, tok, 1))
				parse_err(cfg, (char *)"Bad directory name.");
			break;
		case 2: // "DEV_DIR_MTX"
			if (get_next_token(cfg, &tok))
				break;
			if (fmd_v_str(&cfg->dd_mtx_fn, tok, 1))
				parse_err(cfg, (char *)"Bad directory name.");
			break;
		case 3: // "MPORT"
			parse_mport_info(cfg);
			break;
		case 4: // "MASTER_INFO"
			parse_master_info(cfg);
			break;
		case 5: // "ENDPOINT"
			parse_endpoint(cfg);
			break;
		case 6: // "SWITCH"
			parse_switch(cfg);
			break;
		case 7: // "CONNECT"
			parse_connect(cfg);
			break;
		case 8: // "EOF"
			DBG((char *)"\n");
			goto exit;
			break;
		default:
			parse_err(cfg, (char *)"Unknown parameter.");
			break;
		};
		tok = try_get_next_token(cfg);
	};
exit:
	free(line);
};
	
void fmd_process_cfg_file(struct fmd_cfg_parms *cfg)
{
	if (NULL == cfg->fmd_cfg)
		return;

	INFO("\nFMD: Openning configuration file \"%s\"...\n", cfg->fmd_cfg);
	cfg->fmd_cfg_fd = fopen(cfg->fmd_cfg, "r");
	if (NULL == cfg->fmd_cfg_fd) {
		CRIT("FMD: Config file open failed, errno %d : %s\n",
				errno, strerror(errno));
		cfg->init_and_quit = 1;
		return;
	};

	DBG("\nFMD: Config file contents:");
	fmd_parse_cfg(cfg);

	fclose(cfg->fmd_cfg_fd);
	cfg->fmd_cfg_fd = NULL;
	if (cfg->fmd_cfg_fd) {
		ERR("FMD: Config file close failed, errno %d : %s\n",
				errno, strerror(errno));
		cfg->init_and_quit = 1;
	};
};

struct fmd_cfg_sw *find_cfg_sw_by_ct(uint32_t ct, struct fmd_cfg_parms *cfg)
{
	struct fmd_cfg_sw *ret = NULL;
	uint32_t i;

	for (i = 0; i < cfg->sw_cnt; i++) {
		if ((uint32_t)cfg->sws[i].ct == ct) {
			ret = &cfg->sws[i];
			break;
		};
	};

	return ret;
};

struct fmd_cfg_ep *find_cfg_ep_by_ct(uint32_t ct, struct fmd_cfg_parms *cfg)
{
	struct fmd_cfg_ep *ret = NULL;
	uint32_t i, p;

	for (i = 0; (i < cfg->ep_cnt) && (NULL == ret); i++) {
		if (!cfg->eps[i].valid)
			continue;
		for (p = 0; (p < FMD_MAX_EP_PORT) && (NULL == ret); p++) {
			if (!cfg->eps[i].ports[p].valid)
				continue;
			if (cfg->eps[i].ports[p].ct == ct) {
				ret = &cfg->eps[i];
				break;
			};
		};
	};

	for (i = 0; (i < cfg->max_mport_info_idx) && (NULL == ret); i++) {
		if (cfg->mport_info[i].ct == ct)
			ret = cfg->mport_info[i].ep;
	};

	return ret;
};

int cfg_find_mport(uint32_t mport, struct cfg_mport_info *mp)
{
	unsigned int i;

	for (i = 0; i < cfg->max_mport_info_idx; i++) {
		if (mport != cfg->mport_info[i].num)
			continue;

		mp->num = cfg->mport_info[i].num;
		mp->ct = cfg->mport_info[i].ct;
		mp->op_mode = cfg->mport_info[i].op_mode;
		memcpy(mp->devids, cfg->mport_info[i].devids,
			sizeof(mp->devids));
		return 0;
	};
	return 1;
};

int cfg_get_mp_mem_sz(uint32_t mport, uint8_t *mem_sz)
{
	unsigned int i;

	for (i = 0; i < cfg->max_mport_info_idx; i++) {
		if (mport != cfg->mport_info[i].num)
			continue;

		*mem_sz = cfg->mport_info[i].mem_sz;
		return 0;
	};
	return 1;
};

int fill_in_dev_from_ep(struct cfg_dev *dev, struct int_cfg_ep *ep)
{
	if (!ep->ports[0].valid)
		goto fail;

	dev->name = ep->name;
	dev->port_cnt = ep->port_cnt;
	dev->did_sz = CFG_DEV08;
	dev->is_sw = 0;
	dev->ct = ep->ports[0].ct;
	dev->hc = ep->ports[0].devids[CFG_DEV08].hc;
	dev->did = ep->ports[0].devids[CFG_DEV08].devid;
	dev->ep_pt.valid = 1;
	dev->ep_pt.port = 0;
	dev->ep_pt.ct = dev->ct;
	memcpy(dev->ep_pt.devids, ep->ports[0].devids,
		sizeof(dev->ep_pt.devids));

	return 0;
fail:
	return 1;
};

int fill_in_dev_from_sw(struct cfg_dev *dev, struct int_cfg_sw *sw)
{
	int i;

	dev->name = sw->name;
	dev->port_cnt = CFG_MAX_SW_PORT;
	dev->did_sz = CFG_MAX_SW_PORT;
	dev->did = sw->did;
	dev->hc = sw->hc;
	dev->ct = sw->ct;
	dev->is_sw = 1;
	dev->sw_info.num_ports = CFG_MAX_SW_PORT;
	for (i = 0; i < CFG_MAX_SW_PORT; i++) {
		dev->sw_info.sw_pt[i].valid =
			sw->ports[i].valid;
		dev->sw_info.sw_pt[i].port =
			sw->ports[i].port;
	};
	dev->sw_info.rt[CFG_DEV08] = &sw->rt[CFG_DEV08];
	dev->sw_info.rt[CFG_DEV16] = &sw->rt[CFG_DEV16];
	dev->sw_info.rt[CFG_DEV32] = &sw->rt[CFG_DEV32];

	return 0;
};

int cfg_find_dev_by_ct(uint32_t ct, struct cfg_dev *dev)
{
	struct int_cfg_ep *ep;
	struct int_cfg_sw *sw;

	ep = find_cfg_ep_by_ct(ct, cfg);

	if (NULL != ep)
		return fill_in_dev_from_ep(dev, ep);

	sw = find_cfg_sw_by_ct(ct, cfg);

	if (NULL != sw)
		return fill_in_dev_from_sw(dev, sw);

	return 1;
};

extern int cfg_get_conn_dev(uint32_t ct, int pt,
		struct cfg_dev *dev, int *conn_pt)
{
	struct int_cfg_conn *conn = NULL;
	int conn_end = -1, oe;
	struct int_cfg_ep *ep;

	ep = find_cfg_ep_by_ct(ct, cfg);

	if (NULL != ep) {
		if (pt)
			goto fail;
		conn = ep->ports[0].conn;
		conn_end = ep->ports[0].conn_end;
	}

	if (NULL == ep) {
		struct int_cfg_sw *sw;

		sw = find_cfg_sw_by_ct(ct, cfg);
		if (NULL != sw) {
			if (pt >= CFG_MAX_SW_PORT)
				goto fail;
			if (!sw->ports[pt].valid)
				goto fail;
			if (sw->ports[pt].port != pt)
				goto fail;
			conn = sw->ports[pt].conn;
			conn_end = sw->ports[pt].conn_end;
		};
	};

	if (NULL == conn)
		goto fail;

	if (!conn->valid)
		goto fail;

	oe = OTHER_END(conn_end);
	if (conn_end > 1)
		goto fail;

	*conn_pt = conn->ends[oe].port_num;
	if (conn->ends[oe].ep)
		return fill_in_dev_from_ep(dev, conn->ends[oe].ep_h);
	else
		return fill_in_dev_from_sw(dev, conn->ends[oe].sw_h);

fail:
	return 1;
	
};

#ifdef __cplusplus
}
#endif

