/* CLI Commands for LIBRSKT Application Library */
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <netinet/in.h>
#include "memops.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "librskt_buff.h"
#include "librskt_socket.h"
#include "librskt_list.h"
#include "librskt_info.h"
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt_test.h"
#include "librskt_threads.h"
#include "libcli.h"

void librskt_display_skt(struct cli_env *env, rskt_h skt_h, int row, int header)
{
	struct rskt_socket_t *skt;
	enum rskt_state st;

	skt = rsktl_sock_ptr(&lib.skts, skt_h);
	st = rsktl_get_st(&lib.skts, skt_h);

	if (!row) {
/*
State    : ## 01234567         Bytes      Trans   Ptr
MaxBkLog : ##           TX 0x012345678 0x01234567 0x0123456789ABCDEF
                        RX 0x012345678 0x01234567 0x0123456789ABCDEF

Sa.ct    : 01234567     Sz M 0x01234567
Sa.sn    : 01234567        C 0x01234567        Loc TX W P 0x01234567
sai.sa.ct: 01234567        B 0x01234567                 F 0x01234567
sai.sa.sn: 01234567                                RX R P 0x01234567
                                                        F 0x01234567
msoh    :  V 01234567  "0123456 NAME 34567890"
msh     :  V 01234567  "0123456 NAME 34567890" Rem RX W P 0x01234567
msubh   :  V 01234567                                   F 0x01234567
con msh :    01234567  "0123456 NAME 34567890"     TX R P 0x01234567
con msub:    01234567                                   F 0x01234567
*/
		/* Columnar display */
		if (header) {
        		sprintf(env->output, "\n");
        		logMsg(env);
		};
		if (NULL == skt)
			goto exit;

		sprintf(env->output, 
			"State    : %2d %8s         Bytes      Trans   Ptr\n",
			st, SKT_STATE_STR(st));
		logMsg(env);
		sprintf(env->output, 
			"MaxBkLog : %2d           TX 0x%8x 0x%8x 0x%p\n",
				skt->max_backlog, 
				skt->stats.tx_bytes, 
				skt->stats.tx_trans, 
				skt->tx_buf); 
		logMsg(env);

		sprintf(env->output, 
			"                        RX 0x%8x 0x%8x 0x%p\n\n",
				skt->stats.rx_bytes, 
				skt->stats.rx_trans, 
				skt->rx_buf); 
		logMsg(env);

		sprintf(env->output, 
			"Sa.ct    : %8d     Sz M 0x%8x\n", skt->sa.ct,
			skt->msub_sz);
		logMsg(env);

		sprintf(env->output, 
		"Sa.sn    : %8d        C 0x%8x        Loc TX W P 0x%8x\n",
			skt->sa.sn, 
			skt->con_sz,
			(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_tx_wr_ptr));
		logMsg(env);

		sprintf(env->output, 
		"sai.sa.ct: %8d        B 0x%8x                 F 0x%8x\n",
			skt->sai.sa.ct, 
			skt->buf_sz,
			(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_tx_wr_flags));
		logMsg(env);

		sprintf(env->output, 
		"sai.sa.sn: %8d                                RX R P 0x%8x\n",
			skt->sai.sa.sn, 
			(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_rx_rd_ptr));
		logMsg(env);

		sprintf(env->output, 
	"                                                        F 0x%8x\n",
			(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_rx_rd_flags));
		logMsg(env);

		sprintf(env->output, 
		"msoh    :  %1s %8x  \"%20s\"\n",
			(skt->msoh_valid)?"V":"-",
			(int)skt->msoh,
			skt->msoh_name);
		logMsg(env);

		sprintf(env->output, 
		"msh     :  %1s %8x  \"%20s\"  Rem RX W P 0x%8x\n",
			(skt->msh_valid)?"V":"-",
			(int)skt->msh,
			skt->msh_name,
			(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_rx_wr_ptr));
		logMsg(env);

		sprintf(env->output, 
	"msubh   :  %1s %8x                                   F 0x%8x\n",
			(skt->msubh_valid)?"V":"-",
			(int)skt->msubh,
			(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_rx_wr_flags));
		logMsg(env);

		sprintf(env->output, 
		"con msh :    %8x  \"%20s\"      TX R P 0x%8x\n",
			(int)skt->con_msh,
			skt->con_msh_name,
			(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_tx_rd_ptr));
		logMsg(env);

		sprintf(env->output, 
		"con msub:    %8x                                   F 0x%8x\n",
			(int)skt->con_msubh,
			(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_tx_rd_flags));
		logMsg(env);

		goto exit;
	};

	if (header) {
        	sprintf(env->output, "State  CT  SN   Size  BuffSize  REM MS NAME  Size\n");

	} else {
        	sprintf(env->output, "%6s %4d %4d %6d %8x %15s 0x%x\n", 
			SKT_STATE_STR(st), skt->sa.ct, skt->sa.sn, 
			skt->msub_sz, skt->buf_sz,
			skt->con_msh_name, skt->con_sz);
	};
        logMsg(env);
exit:
	return;
};
			
extern struct cli_cmd RSKTLStatus;

int RSKTLStatusCmd(struct cli_env *env, int argc, char **argv)
{
	int got_one = 0;
	uint64_t i;

	if (argc)
		goto show_help;
        sprintf(env->output, "\nRSKTD  PortNo  MpNum  InitOk   MP Fd Addr\n");
        logMsg(env);
        sprintf(env->output,   "      %8d %5d %8d %2s %2d %s\n",
		lib.portno, lib.mpnum, lib.init_ok, lib.use_mport?"Y":"N", 
		lib.fd, lib.addr.sun_path);
        logMsg(env);

	for (i = 0; i < RSKTS_PER_BLOCK; i++) {
		if (LIBRSKT_H_INVALID == lib.skts.skts[i].handle) {
			continue;
		};
		if (rskt_connected != lib.skts.skts[i].st) {
			continue;
		};
		if (!got_one) {
			librskt_display_skt(env, lib.skts.skts[i].handle, 1, 1);
			got_one = 1;
		};
		librskt_display_skt(env, lib.skts.skts[i].handle, 1, 0);
	};

	if (!got_one) {
        	sprintf(env->output, "\nNo sockets connected!\n");
        	logMsg(env);
	};

        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &RSKTLStatus);

        return 0;
};

struct cli_cmd RSKTLStatus = {
"rsktstatus",
4,
0,
"RSKT Library status command.",
"No Parameters.\n",
RSKTLStatusCmd,
ATTR_NONE
};


extern struct cli_cmd RSKTLibinit;

int RSKTLibInitCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	int portno;
	int mp;

	portno = getDecParm(argv[0], DFLT_DMN_LSKT_SKT);
	mp = getDecParm(argv[1], DFLT_DMN_LSKT_MPORT);

	if (argc > 2)
		goto print_help;

	sprintf(env->output, "\nportno is %d,\nmport is %d\n", portno, mp); 
	logMsg(env);
	sprintf(env->output, "Calling librskt_init...\n"); 
	logMsg(env);
	rc = librskt_init(portno, mp);
	sprintf(env->output, "libskt_init rc = %d, errno = %d:%s\n",
			rc, errno, strerror(errno)); 
	logMsg(env);

	return 0;
print_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &RSKTLibinit);

        return 0;
};

struct cli_cmd RSKTLibinit = {
"linit",
5,
2,
"Initialize local RSKT library",
"<u_skt> <u_mp> <test>\n" 
        "<u_skt>: AF_LOCAL/AF_UNIX socket supported by RSKTD\n"
        "<u_mp>: Local mport number supported by RSKTD\n",
RSKTLibInitCmd,
ATTR_NONE
};

void display_buff(struct cli_env *env, const char *label, int disp_tx,
		volatile struct rskt_buf_hdr *buf_hdr,
		volatile uint8_t *buf, uint32_t buf_size)
{
	uint32_t start, end;
	uint32_t offset;

	if (disp_tx) {
		start = ntohl(buf_hdr->rem_tx_rd_ptr);
		end = ntohl(buf_hdr->loc_tx_wr_ptr);
	} else {
		start = ntohl(buf_hdr->loc_rx_rd_ptr);
		end = ntohl(buf_hdr->rem_rx_wr_ptr);
	};

	if (!start && !end) {
		sprintf(env->output, "No data to display\n"); 
		logMsg(env);
	};

	sprintf(env->output, 
		"   Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
	logMsg(env);
	if (!(start & 0xF)) {
		sprintf(env->output, "\n%2s %6x", label, start & (!0xF));
		logMsg(env);
		for (offset = 0; offset < (start & 0xF); offset++) {
			sprintf(env->output, "   ");
			logMsg(env);
		};
	};
	if (start > end) {
		for (offset = start; offset < buf_size; offset++) {
			if (!(offset & 0xF)) {
				sprintf(env->output,"\n%2s %6x", label, offset);
				logMsg(env);
			};
			sprintf(env->output, " %2x", *(buf + offset));
			logMsg(env);
		};
		start = 0;
	};
	for (offset = start; offset < end; offset++) {
		if (!(offset & 0xF)) {
			sprintf(env->output,"\n%2s %6x", label, offset);
			logMsg(env);
		};
		sprintf(env->output, " %2x", *(buf + offset));
		logMsg(env);
	};
	sprintf(env->output, "\n");
	logMsg(env);
};


extern struct cli_cmd RSKTDataDump;

int RSKTDataDumpCmd(struct cli_env *env, int argc, char **argv)
{
	rskt_h skt_h;
	struct rskt_socket_t *skt;
	enum rskt_state st;

	if (argc > 1) {
		goto print_help;
	};

	skt_h = getHex(argv[0], 0);

	skt = rsktl_sock_ptr(&lib.skts, skt_h);
	if (NULL == skt) {
		sprintf(env->output, "Socket handle 0x%lx Unknown\n", skt_h);
		logMsg(env);
		goto exit;
	};

	st = rsktl_get_st(&lib.skts, skt_h);
		
	sprintf(env->output, "State : %d - \"%s\"\n", st, SKT_STATE_STR(st));
	logMsg(env);
	sprintf(env->output, "Debug : %d\n", skt->debug);
	logMsg(env);
	sprintf(env->output, "Tx Bs : %d\n", skt->stats.tx_bytes);
	logMsg(env);
	sprintf(env->output, "Rx Bs : %d\n", skt->stats.rx_bytes);
	logMsg(env);
	sprintf(env->output, "Tx Ts : %d\n", skt->stats.tx_trans);
	logMsg(env);
	sprintf(env->output, "Rx Ts : %d\n", skt->stats.rx_trans);
	logMsg(env);
	sprintf(env->output, "MxBklg:%d\n", skt->max_backlog);
	logMsg(env);
	sprintf(env->output, "MsubSz:%d %x\n", skt->msub_sz, skt->msub_sz);
	logMsg(env);
	if (skt->msub_sz) {
		if (NULL == skt->hdr) {
			sprintf(env->output, "Hdr: NULL\n");
		} else {
			sprintf(env->output, 
				"LocHdr: %16p Rd %8x Wr %8x Lf %8x Rf %8x\n", 
				skt->hdr,
				ntohl(skt->hdr->loc_tx_wr_ptr),
				ntohl(skt->hdr->loc_tx_wr_flags),
				ntohl(skt->hdr->loc_rx_rd_ptr),
				ntohl(skt->hdr->loc_rx_rd_flags));
			logMsg(env);
			sprintf(env->output,
				"RemHdr: %16p Rd %8x Wr %8x Lf %8x Rf %8x\n", 
				&skt->hdr->rem_rx_wr_ptr,
				ntohl(skt->hdr->rem_rx_wr_ptr),
				ntohl(skt->hdr->rem_rx_wr_flags),
				ntohl(skt->hdr->rem_tx_rd_ptr),
				ntohl(skt->hdr->rem_tx_rd_flags));
		};
		logMsg(env);
		if (NULL == skt->tx_buf) {
			sprintf(env->output, "TxBuf: NULL\n");
			logMsg(env);
		} else {
			display_buff(env, "TX", 1, skt->hdr, 
					skt->tx_buf, skt->buf_sz);
		};
		if (NULL == skt->rx_buf) {
			sprintf(env->output, "RxBuf: NULL\n");
			logMsg(env);
		} else {
			display_buff(env, "RX", 0, skt->hdr, 
					skt->rx_buf, skt->buf_sz);
		};
	};

	sprintf(env->output, "ConSz:%d %x\n", skt->con_sz, skt->con_sz);
	logMsg(env);
exit:
	return 0;

print_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &RSKTDataDump);
        return 0;
};

struct cli_cmd RSKTDataDump = {
"lddump",
3,
1,
"Dump socket buffer values",
"<skt_h> Socket handle value, hexadecimal\n",
RSKTDataDumpCmd,
ATTR_RPT
};

struct cli_cmd *rskt_lib_cmds[] =

        { &RSKTLStatus,
	  &RSKTLibinit,
	  &RSKTDataDump
        };

void librskt_bind_cli_cmds(void)
{
        add_commands_to_cmd_db(sizeof(rskt_lib_cmds)/sizeof(rskt_lib_cmds[0]),
                                rskt_lib_cmds);

        return;
};

#ifdef __cplusplus
}
#endif
