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
#include "librskt_private.h"
#include "librsktd_private.h"
#include "librskt_test.h"
#include "libcli.h"
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

void librskt_display_skt(struct cli_env *env, struct rskt_handle_t *skt_h,
                        int row, int header)
{
	struct rskt_socket_t *skt;

	sem_wait(&lib.skts_mtx);
	skt = (struct rskt_socket_t *)skt_h->skt;

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
			LOGMSG(env, "\n");
		}
		if (NULL == skt)
			goto exit;

		LOGMSG(env,
				"State    : %2d %8s         Bytes      Trans   Ptr\n",
				skt_h->st, SKT_STATE_STR(skt_h->st));
		LOGMSG(env, "MaxBkLog : %2d           TX 0x%8x 0x%8x 0x%p\n",
				skt->max_backlog, skt->stats.tx_bytes,
				skt->stats.tx_trans, skt->tx_buf);

		LOGMSG(env, "                        RX 0x%8x 0x%8x 0x%p\n\n",
				skt->stats.rx_bytes, skt->stats.rx_trans,
				skt->rx_buf);

		LOGMSG(env, "Sa.ct    : %8d     Sz M 0x%8x\n", skt_h->sa.ct,
				skt->msub_sz);

		LOGMSG(env,
				"Sa.sn    : %8d        C 0x%8x        Loc TX W P 0x%8x\n",
				skt_h->sa.sn, skt->con_sz,
				(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_tx_wr_ptr));

		LOGMSG(env,
				"sai.sa.ct: %8d        B 0x%8x                 F 0x%8x\n",
				skt->sai.sa.ct, skt->buf_sz,
				(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_tx_wr_flags));

		LOGMSG(env,
				"sai.sa.sn: %8d                                RX R P 0x%8x\n",
				skt->sai.sa.sn,
				(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_rx_rd_ptr));

		LOGMSG(env,
				"                                                        F 0x%8x\n",
				(NULL == skt->hdr)?0:ntohl(skt->hdr->loc_rx_rd_flags));

		LOGMSG(env, "msoh    :  %1s %8x  \"%20s\"\n",
				(skt->msoh_valid) ? "V" : "-", (int )skt->msoh,
				skt->msoh_name);

		LOGMSG(env, "msh     :  %1s %8x  \"%20s\"  Rem RX W P 0x%8x\n",
				(skt->msh_valid) ? "V" : "-", (int )skt->msh,
				skt->msh_name,
				(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_rx_wr_ptr));

		LOGMSG(env,
				"msubh   :  %1s %8x                                   F 0x%8x\n",
				(skt->msubh_valid) ? "V" : "-",
				(int )skt->msubh,
				(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_rx_wr_flags));

		LOGMSG(env, "con msh :    %8x  \"%20s\"      TX R P 0x%8x\n",
				(int )skt->con_msh, skt->con_msh_name,
				(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_tx_rd_ptr));

		LOGMSG(env,
				"con msub:    %8x                                   F 0x%8x\n",
				(int )skt->con_msubh,
				(NULL == skt->hdr)?0:ntohl(skt->hdr->rem_tx_rd_flags));

		goto exit;
	};

	if (NULL == skt)
		goto exit;

	if (header) {
		LOGMSG(env, "State  CT  SN   Size  BuffSize  REM MS NAME  Size\n");
	} else {
		LOGMSG(env, "%6s %4d %4d %6d %8x %15s 0x%x\n",
			SKT_STATE_STR(skt_h->st), skt_h->sa.ct, skt_h->sa.sn, 
			skt->msub_sz, skt->buf_sz,
			skt->con_msh_name, skt->con_sz);
	}

exit:
	sem_post(&lib.skts_mtx);
};
			
extern struct cli_cmd RSKTLStatus;

int RSKTLStatusCmd(struct cli_env *env, int argc, char **argv)
{
	rskt_h skt_h;
	struct l_item_t *li;
	int got_one = 0;

	if (argc)
		goto show_help;
	LOGMSG(env, "\nRSKTD  PortNo  MpNum  InitOk   Fd Addr\n");
	LOGMSG(env,   "      %8d %5d %8d %3d %s\n",
		lib.portno, lib.mpnum, lib.init_ok, lib.fd,
		lib.addr.sun_path);

	skt_h = (rskt_h)l_head(&lib.skts, &li);
	while (NULL != skt_h) {
		if (!got_one) {
			librskt_display_skt(env, skt_h, 1, 1);
			got_one = 1;
		};
		librskt_display_skt(env, skt_h, 1, 0);
		skt_h = (rskt_h)l_next(&li);
	};

	if (!got_one) {
		LOGMSG(env, "\nNo sockets connected!\n");
	}

	return 0;

show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
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

extern struct cli_cmd RSKTLTest;

int RSKTLTestCmd(struct cli_env *env, int argc, char **argv)
{

	if (argc > 1) {
		LOGMSG(env, "\nFAILED: Extra/invalid values: %s\n", argv[1]);
		cli_print_help(env, &RSKTLStatus);
	}

	if (argc) {
		lib.test = getHex(argv[0], 0);
	}
	
	LOGMSG(env, "\nTest : %d\n", lib.test);

	return 0;
};

struct cli_cmd RSKTLTest = {
"rskttest",
5,
0,
"RSKT Library Controls test parameter",
"<tst>\n"
	"<tst> : 0 - message to rsktd, 1 - fake/test messaging\n",
RSKTLTestCmd,
ATTR_NONE
};

struct librskt_rsktd_to_app_msg trx;
uint32_t test_err;

extern struct cli_cmd RSKTLResp;

int RSKTLRespCmd(struct cli_env *env, int argc, char **argv)
{
	if (!argc)
		goto display;

	if (argc < 2)
		goto print_help;

	test_err = getHex(argv[0], 1);
	trx.msg_type = (getDecParm(argv[1], 0) | LIBRSKTD_RESP);
	if (trx.msg_type == LIBRSKTD_ACCEPT_RESP) {
		if (argc != 8) 
			goto print_help;
		trx.a_rsp.msg.accept.new_sn = getDecParm(argv[2], 1);
		trx.a_rsp.msg.accept.peer_sa.ct = getDecParm(argv[3], 1);
		trx.a_rsp.msg.accept.peer_sa.sn = getDecParm(argv[4], 1);
		trx.a_rsp.msg.accept.ms_size = getDecParm(argv[5], 1);
		memcpy(trx.a_rsp.msg.accept.mso_name, argv[6], strlen(argv[6]));
		memcpy(trx.a_rsp.msg.accept.ms_name, argv[7], strlen(argv[7]));
	};
	if (trx.msg_type == LIBRSKTD_CONN_RESP) {
		if (argc != 8) 
			goto print_help;
		trx.a_rsp.msg.conn.new_sn = getDecParm(argv[2], 1);
		memcpy(trx.a_rsp.msg.conn.mso, argv[3], strlen(argv[3]));
		memcpy(trx.a_rsp.msg.conn.ms, argv[4], strlen(argv[4]));
		trx.a_rsp.msg.conn.msub_sz = getHex(argv[5], 0);
		memcpy(trx.a_rsp.msg.conn.rem_ms, argv[6], strlen(argv[7]));
		trx.a_rsp.msg.conn.rem_sn = getDecParm(argv[7], 1);
	};
display:
	LOGMSG(env, "\nResp err: %d\n", test_err);
	LOGMSG(env, "Resp msg type: 0x%x 0x%x\n", trx.msg_type,
			ntohl(trx.msg_type));

	if (LIBRSKTD_ACCEPT_RESP == trx.msg_type) {
		LOGMSG(env, "New_sn : %d\n", trx.a_rsp.msg.accept.new_sn);
		LOGMSG(env, "peer_sa.ct: %d\n", trx.a_rsp.msg.accept.peer_sa.ct);
		LOGMSG(env, "peer_sa.sn: %d\n", trx.a_rsp.msg.accept.peer_sa.sn);
		LOGMSG(env, "mso_name %s\n", trx.a_rsp.msg.accept.mso_name);
		LOGMSG(env, "ms_name %s\n", trx.a_rsp.msg.accept.ms_name);
	}
	
	if (LIBRSKTD_CONN_RESP == trx.msg_type) {
		LOGMSG(env, "New_sn: %d\n", trx.a_rsp.msg.conn.new_sn);
		LOGMSG(env, "mso   : %s\n", trx.a_rsp.msg.conn.mso);
		LOGMSG(env, "ms    : %s\n", trx.a_rsp.msg.conn.ms);
		LOGMSG(env, "msub_s: %x\n", trx.a_rsp.msg.conn.msub_sz);
		LOGMSG(env, "rem_ms: %s\n", trx.a_rsp.msg.conn.rem_ms);
		LOGMSG(env, "rem_sn: %d\n", trx.a_rsp.msg.conn.rem_sn);
	}
	
	return 0;

print_help:
	LOGMSG(env, "\nFAILED: Extra/invalid values\n");
	cli_print_help(env, &RSKTLResp);
	return 0;
};

struct cli_cmd RSKTLResp = {
"rsktresp",
5,
0,
"RSKT Library Controls test parameter",
"<err> <msg_type> <msg parms>\n"
	"<err> : Standard error code for response message\n"
	"<msg_type> : Bind 1 listen 2 accept 3 connect 4 close 5 hello 1111\n"
	"accept : new_sn ct sn ms_size mso_name ms_name\n"
	"connect: new_sn mso ms msub_s con_ms con_sn\n",
RSKTLRespCmd,
ATTR_NONE
};

rskt_h a_skt_h;  /* Socket used for bind, listen, and accept */
rskt_h t_skt_h; /* Socket returned by accept, and used by connect/write/read */

void check_test_socket(struct cli_env *env, rskt_h *skt_h)
{
	if (NULL == *skt_h) {
		LOGMSG(env, "Allocating test socket\n");
		*skt_h = rskt_create_socket();
	}
};

extern struct cli_cmd RSKTLibinit;

int RSKTLibInitCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	int portno;
	int mp;
	int test;

	portno = getDecParm(argv[0], RSKT_DFLT_APP_PORT_NUM);
	mp = getDecParm(argv[1], DFLT_DMN_LSKT_MPORT);
	test = getDecParm(argv[2], 0);

	if (argc > 3)
		goto print_help;

	LOGMSG(env, "\nportno is %d,\nmport is %d\ntest is %d\n", portno, mp,
			test);
	LOGMSG(env, "Calling librskt_init...\n");
	librskt_test_init(test);
	rc = librskt_init(portno, mp);
	LOGMSG(env, "libskt_init rc = %d, errno = %d:%s\n", rc, errno,
			strerror(errno));

	return 0;

print_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTLibinit);

	return 0;
};

struct cli_cmd RSKTLibinit = {
"linit",
5,
3,
"Initialize local RSKT library",
"<u_skt> <u_mp> <test>\n" 
        "<u_skt>: AF_LOCAL/AF_UNIX socket supported by RSKTD\n"
        "<u_mp>: Local mport number supported by RSKTD\n"
        "<test>: 1 - Test mode, no messaging to RSKTD \n",
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
		LOGMSG(env, "No data to display\n");
	};

	LOGMSG(env, "   Offset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
	if (!(start & 0xF)) {
		LOGMSG(env, "\n%2s %6x", label, start & (!0xF));
		for (offset = 0; offset < (start & 0xF); offset++) {
			LOGMSG(env, "   ");
		};
	};
	if (start > end) {
		for (offset = start; offset < buf_size; offset++) {
			if (!(offset & 0xF)) {
				LOGMSG(env, "\n%2s %6x", label, offset);
			};
			LOGMSG(env, " %2x", *(buf + offset));
		};
		start = 0;
	};
	for (offset = start; offset < end; offset++) {
		if (!(offset & 0xF)) {
			LOGMSG(env, "\n%2s %6x", label, offset);
		};
		LOGMSG(env, " %2x", *(buf + offset));
	};
	LOGMSG(env, "\n");
}

extern struct cli_cmd RSKTDataDump;

int RSKTDataDumpCmd(struct cli_env *env, int argc, char **argv)
{
	struct rskt_socket_t *skt;

	if (argc)
		goto print_help;

	check_test_socket(env, &t_skt_h);
	if (NULL == t_skt_h) {
		// messages have been logged
		goto exit;
	}

	skt = (struct rskt_socket_t *)t_skt_h->skt;
	LOGMSG(env, "State : %d - \"%s\"\n", t_skt_h->st,
			SKT_STATE_STR(t_skt_h->st));
	LOGMSG(env, "Debug : %d\n", skt->debug);
	LOGMSG(env, "Tx Bs : %d\n", skt->stats.tx_bytes);
	LOGMSG(env, "Rx Bs : %d\n", skt->stats.rx_bytes);
	LOGMSG(env, "Tx Ts : %d\n", skt->stats.tx_trans);
	LOGMSG(env, "Rx Ts : %d\n", skt->stats.rx_trans);
	LOGMSG(env, "MxBklg:%d\n", skt->max_backlog);
	LOGMSG(env, "MsubSz:%d %x\n", skt->msub_sz, skt->msub_sz);
	if (skt->msub_sz) {
		if (NULL == skt->hdr) {
			LOGMSG(env, "Hdr: NULL\n");
		} else {
			LOGMSG(env,
					"LocHdr: %16p Rd %8x Wr %8x Lf %8x Rf %8x\n",
					skt->hdr,
					ntohl(skt->hdr->loc_tx_wr_ptr),
					ntohl(skt->hdr->loc_tx_wr_flags),
					ntohl(skt->hdr->loc_rx_rd_ptr),
					ntohl(skt->hdr->loc_rx_rd_flags));
			LOGMSG(env,
					"RemHdr: %16p Rd %8x Wr %8x Lf %8x Rf %8x\n",
					&skt->hdr->rem_rx_wr_ptr,
					ntohl(skt->hdr->rem_rx_wr_ptr),
					ntohl(skt->hdr->rem_rx_wr_flags),
					ntohl(skt->hdr->rem_tx_rd_ptr),
					ntohl(skt->hdr->rem_tx_rd_flags));
		};
		if (NULL == skt->tx_buf) {
			LOGMSG(env, "TxBuf: NULL\n");
		} else {
			display_buff(env, "TX", 1, skt->hdr, skt->tx_buf,
					skt->buf_sz);
		};
		if (NULL == skt->rx_buf) {
			LOGMSG(env, "RxBuf: NULL\n");
		} else {
			display_buff(env, "RX", 0, skt->hdr, skt->rx_buf,
					skt->buf_sz);
		}
	}

	LOGMSG(env, "ConSz:%d %x\n", skt->con_sz, skt->con_sz);
	return 0;

print_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTDataDump);

exit:
	return 0;
};

struct cli_cmd RSKTDataDump = {
"lddump",
3,
0,
"Dump t_skt_h socket buffer values",
"No parameters\n",
RSKTDataDumpCmd,
ATTR_RPT
};

extern struct cli_cmd RSKTSocketDump;

int RSKTSocketDumpCmd(struct cli_env *env, int argc, char **argv)
{
	int opt = 1;
	if (argc > 1)
		goto print_help;

	if (argc)
		opt = getHex(argv[0], 0);

	if (opt) {
		check_test_socket(env, &t_skt_h);
		if (NULL == t_skt_h) {
			goto exit;
		}
		LOGMSG(env, "\nDisplay t_skt_h\n");
		librskt_display_skt(env, t_skt_h, 0, 1);
	} else {
		check_test_socket(env, &a_skt_h);
		if (NULL == a_skt_h) {
			goto exit;
		}

		LOGMSG(env, "\nDisplay a_skt_h\n");
		librskt_display_skt(env, a_skt_h, 0, 1);
	};

exit:
	return 0;

print_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTSocketDump);
	return 0;
};

struct cli_cmd RSKTSocketDump = {
"lsdump",
3,
0,
"Dump t_skt_h socket statusvalues",
"{<t_skt_h>}\n"
	"<t_skt_h>: Option parameter, 0 means a_skt_h, 1 means t_skt_h\n",
RSKTSocketDumpCmd,
ATTR_RPT
};

extern struct cli_cmd RSKTBind;

int RSKTBindCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	struct rskt_sockaddr sock_addr;

	if (argc > 1)
		goto show_help;

	check_test_socket(env, &a_skt_h);
	if (NULL == a_skt_h) {
		// messages have been logged
		goto exit;
	}

	sock_addr.ct = 0;
	sock_addr.sn = getDecParm(argv[0], 1);

	LOGMSG(env, "Addr: Comp_tag %d socknum %d\n", sock_addr.ct,
			sock_addr.sn);
	LOGMSG(env, "Attempting bind...\n");

	rc = rskt_bind(a_skt_h, &sock_addr);
	LOGMSG(env, "%s,%u: rc = %d, errno = %d:%s\n", __func__, __LINE__, rc,
			errno, strerror(errno));
	librskt_display_skt(env, a_skt_h, 0, 1);
	return 0;

show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTBind);

exit:
	return 0;
};

struct cli_cmd RSKTBind = {
"lbind",
5,
1,
"Test bind message to RDMA Daemon",
"<skt>\n"
	"<skt> is the socket to bind to...\n",
RSKTBindCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTListen;

int RSKTListenCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	int max_backlog;

	if (argc > 1)
		goto show_help;

	check_test_socket(env, &a_skt_h);
	if (NULL == a_skt_h) {
		// messages have been logged
		return 0;
	}

	max_backlog = getDecParm(argv[0], 1);

	LOGMSG(env, "Backlog: %d\n", max_backlog);
	LOGMSG(env, "Attempting listen...\n");

	rc = rskt_listen(a_skt_h, max_backlog);
	LOGMSG(env, "%s,%u: rc = %d, errno = %d:%s\n", __func__, __LINE__, rc,
			errno, strerror(errno));
	librskt_display_skt(env, a_skt_h, 0, 1);
	return 0;
show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTListen);

	return 0;
};

struct cli_cmd RSKTListen = {
"llisten",
2,
1,
"Test bind message to RDMA Daemon",
"<bklog>\n"
	"<bklog> maximum pending connect requests for this socket...\n",
RSKTListenCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTAccept;

int RSKTAcceptCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	int sn;
	struct rskt_sockaddr t_sa;

	if (argc > 1)
		goto show_help;
	check_test_socket(env, &a_skt_h);
	if (NULL == a_skt_h) {
		// messages have been logged
		goto exit;
	}

	sn = getDecParm(argv[0], 1);

	LOGMSG(env, "SockNum: %d\n", sn);
	LOGMSG(env, "Attempting accept...\n");

	rc = rskt_accept(a_skt_h, t_skt_h, &t_sa);
	if (rc) {
		LOGMSG(env, "%s,%u: rc = %d, errno = %d:%s\n", __func__,
				__LINE__, rc, errno, strerror(errno));
	}
	LOGMSG(env, "\nAccepting Socket Status:\n");
	librskt_display_skt(env, a_skt_h, 0, 0);
	LOGMSG(env, "\nConnected Socket Status:\n");
	librskt_display_skt(env, t_skt_h, 0, 0);
	return 0;

show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTAccept);

exit:
        return 0;
};

struct cli_cmd RSKTAccept = {
"laccept",
2,
1,
"Test accept message to RDMA Daemon",
"<sn>\n"
	"<sn> Socket number to accept with...\n",
RSKTAcceptCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTConnect;

int RSKTConnectCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;
	struct rskt_sockaddr t_sa;

	if (argc > 2)
		goto show_help;
	check_test_socket(env, &t_skt_h);
	if (NULL == t_skt_h) {
		// messages have been logged
		goto exit;
	}

	t_sa.ct = getDecParm(argv[0], 1);
	t_sa.sn = getDecParm(argv[1], 1);

	LOGMSG(env, "CompTag: %d\n", t_sa.ct);
	LOGMSG(env, "SockNum: %d\n", t_sa.sn);
	LOGMSG(env, "Attempting connect...\n");

	rc = rskt_connect(t_skt_h, &t_sa);
	if (rc) {
		LOGMSG(env, "%s,%u: rc = %d, errno = %d:%s\n", __func__,
				__LINE__, rc, errno, strerror(errno));
	}
	librskt_display_skt(env, t_skt_h, 0, 1);
	return 0;

show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTConnect);

exit:
	return 0;
};

struct cli_cmd RSKTConnect = {
"lconnect",
2,
2,
"Test connect message to RDMA Daemon",
"<ct> <sn>\n"
	"<ct> Destination ID to connect to\n"
	"<sn> Socket number to connect to...\n",
RSKTConnectCmd,
ATTR_NONE
};

uint32_t rskt_wr_cnt;
uint8_t rskt_wr_data;

extern struct cli_cmd RSKTWrite;
int RSKTWriteCmd(struct cli_env *env, int argc, char **argv)
{
	uint8_t *data_buf;
	uint8_t value;
	uint32_t i, rc;
	uint32_t repeat = 1;

	if (argc) {
		rskt_wr_cnt = getHex(argv[0], 0);
		rskt_wr_data = getHex(argv[1], 0);
		if (argc > 2)
			repeat = getHex(argv[2], 0);
	};

	if ((argc > 3) || (!rskt_wr_cnt) || (!repeat))
		goto show_help;

	value = rskt_wr_data;

	data_buf = (uint8_t *)calloc(1, rskt_wr_cnt);
	if (NULL == data_buf) {
		LOGMSG(env, "Failed to allocate memory for data buffer\n");
		goto exit;
	}

	for (i = 0; i < rskt_wr_cnt; i++)
		data_buf[i] = value--;

	check_test_socket(env, &t_skt_h);
	if (NULL == t_skt_h) {
		// messages have been logged
		free(data_buf);
		goto exit;
	}

	LOGMSG(env, "Byte Cnt: %x\n", rskt_wr_cnt);
	LOGMSG(env, "FirstVal: %x\n", data_buf[0]);
	for (i = 0; i < repeat; i++) {
		do {
			rc = rskt_write(t_skt_h, data_buf, rskt_wr_cnt);
		} while (rc && (1 != repeat));
		LOGMSG(env, "Return Code: %d\n", rc);
	}
	free(data_buf);
	return 0;

show_help:
	LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
	cli_print_help(env, &RSKTWrite);

exit:
	return 0;
};

struct cli_cmd RSKTWrite = {
"rsktwrite",
5,
2,
"Write bytes to socket connection",
"{<byte_cnt> <value> {<repeat>}}}\n"
	"<byte_cnt> Number of bytes to send, must be greater than 0\n"
	"<value> First value to send.\n"
	"        Subsequent bytes count down from this value\n"
	"<repeat> Optionally specify the number of writes to perform\n",
RSKTWriteCmd,
ATTR_RPT
};

uint32_t rskt_rd_cnt;

extern struct cli_cmd RSKTRead;
int RSKTReadCmd(struct cli_env *env, int argc, char **argv)
{
        uint8_t *data_buf;
        int i, rc;
        uint32_t repeat = 1, r;

        if (argc) {
                rskt_rd_cnt = getHex(argv[0], 0);
                if (argc > 1)
                        repeat = getHex(argv[1], 0);
        }

        if ((argc > 2) || !rskt_rd_cnt || !repeat)
                goto show_help;

        check_test_socket(env, &t_skt_h);
        if (NULL == t_skt_h) {
                // messages have been logged
                goto exit;
        }


        LOGMSG(env, "Byte Cnt: %x\n", rskt_rd_cnt);
        data_buf = (uint8_t *)calloc(1, rskt_rd_cnt);
        if (NULL == data_buf) {
                LOGMSG(env, "Failed to allocate memory for data buffer\n");
                goto exit;
        }

        for (r = 0; r < repeat; r++) {
                do {
                        rc = rskt_read(t_skt_h, data_buf, rskt_rd_cnt);
                } while ((rc <= 0) && (1 != repeat));
                LOGMSG(env, "Return Code: %d\n", rc);
                if (rc >0) {
                        LOGMSG(env,
                        "Data: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
                        for (i = 0; i < rc; i++) {
                                if (!(i & 0xF)) {
                                        LOGMSG(env, "\n%5x ", i);
                                }
                                LOGMSG(env, "%2x ", data_buf[i]);
                        };
                        LOGMSG(env, "\n");
                };
        };
        free(data_buf);
        return 0;

show_help:
        LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        cli_print_help(env, &RSKTRead);
exit:
        return 0;
};

struct cli_cmd RSKTRead = {
"rsktread",
5,
1,
"Read bytes from socket connection",
"{<byte_cnt> {<repeat>}}}\n"
	"<byte_cnt> Max bytes to read, must be greater than 0\n"
	"<repeat> Optional, number of times to successfully receive bytes\n",
RSKTReadCmd,
ATTR_RPT
};

extern struct cli_cmd RSKTTxTest;
#define TEST_BUFF_SIZE 4096
int RSKTTxTestCmd(struct cli_env *env, int argc, char **argv)
{
	uint8_t data_buf[TEST_BUFF_SIZE] = {0}, rx_data_buf[TEST_BUFF_SIZE] = {0};
	uint32_t repeat = 1, r, s, sz;
	int check = 1, rc;

	if (argc) {
		repeat = getHex(argv[0], 0);
		if (argc > 1) {
			check = 0;
		}
	};
	if ((argc > 2) || !repeat)
		goto show_help;

	check_test_socket(env, &t_skt_h);
	if (NULL == t_skt_h) {
		// messages have been logged
		goto exit;
	}

	for (r = 0; r < TEST_BUFF_SIZE; r++)
		data_buf[r] = (r % 0xFD) + 1; /* NO 0's, no FF's */

	LOGMSG(env, "\nStarting transmit test...\n");

	for (r = 0; r < repeat; r++) {
		LOGMSG(env, "Repeat %d\n", r);
		for (sz = 1; sz <= TEST_BUFF_SIZE; sz++) {
			if (check)
				memset(rx_data_buf, 0xFF, sz);
			do {
				rc = rskt_write(t_skt_h, data_buf, sz);
			} while (rc && (ETIMEDOUT == errno));
			if (check) {
				LOGMSG(env, "\nWrote Sz %x Err %d\n",
					sz, errno);
				librskt_display_skt(env, t_skt_h, 0, 0);
				fflush(stdout);
				if (errno)
					goto exit;
			};
			do {
				rc = rskt_read(t_skt_h, rx_data_buf, sz);
			} while ((rc <= 0) && (ETIMEDOUT == errno));

			if (rc <=0)
				break;
			if (!check)
				continue;

			if ((uint32_t)rc != sz) {
        			LOGMSG(env, "\nFAILED: Sz %x rc %x\n",
					sz, rc);
				break;
			};
			for (s = 0; s < sz; s++) {
				if (data_buf[s] != rx_data_buf[s]) {
        				LOGMSG(env,
						"\nSz %x idx %x w %2x r %2x\n",
						sz, s, data_buf[s], 
						rx_data_buf[s]);
					goto exit;
				};
			};
		};
	};
				
exit:
        return 0;

show_help:
        LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
        cli_print_help(env, &RSKTTxTest);

        return 0;
};

struct cli_cmd RSKTTxTest = {
"txtest",
6,
0,
"Transmission loopback test",
"{<repeat> {nocheck>}\n"
	"<repeat> Optional number of times to repeat the test, default is 1\n"
	"<nocheck> Any value disables data checking\n",
RSKTTxTestCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTRxTest;
#define TEST_BUFF_SIZE 4096

int RSKTRxTestCmd(struct cli_env *env, int argc, char **argv)
{
        uint8_t data_buf[TEST_BUFF_SIZE], rx_data_buf[TEST_BUFF_SIZE];
	        uint32_t repeat = 1, r, s, sz;
        int check = 1, rc;

        if (argc) {
                repeat = getHex(argv[0], 0);
                if (argc > 1) {
                        check = 0;
		}
        };
        if ((argc > 2) || !repeat)
                goto show_help;

        check_test_socket(env, &t_skt_h);
        if (NULL == t_skt_h) {
                // messages have been logged
                goto exit;
        }

        for (r = 0; r < TEST_BUFF_SIZE; r++)
                data_buf[r] = (r % 0xFD) + 1; /* NO 0's, no FF's */

	LOGMSG(env, "\nStarting Receive Test\n");

        for (r = 0; r < repeat; r++) {
		LOGMSG(env, "Repeat %d\n", r);
                for (sz = 1; sz <= TEST_BUFF_SIZE; sz++) {
                        if (check)
                                memset(rx_data_buf, 0xFF, sz);
                        do {
				rc = rskt_read(t_skt_h, rx_data_buf, sz);
                        } while ((rc <= 0) && (ETIMEDOUT == errno));
			if (check) {
				LOGMSG(env, "\nRead Sz %x %d\n",
					sz, errno);
				librskt_display_skt(env, t_skt_h, 0, 0);
				fflush(stdout);
				if (errno)
					goto exit;
			};
                        if ((uint32_t)rc != sz) {
                                LOGMSG(env, "\nFAILED: Sz %x rc %x\n",
                                        sz, rc);
				break;
                        };
                        do {
                                rc = rskt_write(t_skt_h, rx_data_buf, sz);
                        } while (rc && (ETIMEDOUT == errno));
                        if (!check)
                                continue;
                        for (s = 0; s < sz; s++) {
                                if (data_buf[s] != rx_data_buf[s]) {
                                        LOGMSG(env,
                                                "\nSz %x idx %x w %2x r %2x\n",
                                                sz, s, data_buf[s],
                                                rx_data_buf[s]);
                                        goto exit;
                                };
                        };
                };
        };
exit:
        return 0;
show_help:
        LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
        cli_print_help(env, &RSKTRxTest);

        return 0;
};

struct cli_cmd RSKTRxTest = {
"rxtest",
6,
0,
"Reception loopback test",
"{<repeat> {nocheck>}\n"
        "<repeat> Optional number of times to repeat the test, default is 1\n"
        "<nocheck> Any value disables data checking\n",
RSKTRxTestCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTClose;

int RSKTCloseCmd(struct cli_env *env, int argc, char **argv)
{
	int rc;

        if (argc > 1)
                goto show_help;

	if (argc)
        	rc = rskt_close(a_skt_h);
	else
        	rc = rskt_close(t_skt_h);

        LOGMSG(env, "\nrskt_close returned %d: %d %s\n", rc, errno,
                        strerror(errno));
	if (argc)
        	librskt_display_skt(env, a_skt_h, 0, 0);
	else
        	librskt_display_skt(env, t_skt_h, 0, 0);
        return 0;

show_help:
        LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
        cli_print_help(env, &RSKTClose);

        return 0;
};

struct cli_cmd RSKTClose = {
"lclose",
3,
0,
"Close currently open socket",
"{<acc>}\n"
        "<acc>: If any parameter is entered, the accepting socket is closed.\n",
RSKTCloseCmd,
ATTR_NONE
};

extern struct cli_cmd RSKTDestroy;

int RSKTDestroyCmd(struct cli_env *env, int argc, char **argv)
{
        if (argc > 1)
                goto show_help;
	if (argc)
        	rskt_destroy_socket(&a_skt_h);
	else
        	rskt_destroy_socket(&t_skt_h);

        LOGMSG(env, "\nrskt_destroy completed.\n");
        return 0;

show_help:
        LOGMSG(env, "\nFAILED: Extra parms or invalid values: %s\n", argv[0]);
        cli_print_help(env, &RSKTDestroy);

        return 0;
};

struct cli_cmd RSKTDestroy = {
"ldestroy",
2,
0,
"Destroy socket handle",
"{<acc>}\n"
        "<acc> Any single parameter will destroy the accept handle.\n"
	"      Default is the connected socket handle.\n",
RSKTDestroyCmd,
ATTR_NONE
};

struct cli_cmd *rskt_lib_cmds[] =

        { &RSKTLStatus,
          &RSKTLTest,
          &RSKTLResp,
	  &RSKTLibinit,
	  &RSKTBind,
	  &RSKTListen,
	  &RSKTAccept,
	  &RSKTConnect,
	  &RSKTDataDump,
	  &RSKTSocketDump,
	  &RSKTWrite,
	  &RSKTRead,
	  &RSKTRxTest,
	  &RSKTTxTest,
	  &RSKTClose,
	  &RSKTDestroy
        };

void librskt_bind_cli_cmds(void)
{
	a_skt_h = NULL;
	t_skt_h = NULL;
	rskt_wr_cnt = 16;
	rskt_wr_data = 0x80;
	rskt_rd_cnt = 16;

        add_commands_to_cmd_db(sizeof(rskt_lib_cmds)/sizeof(rskt_lib_cmds[0]),
                                rskt_lib_cmds);

        return;
};

#ifdef __cplusplus
}
#endif
