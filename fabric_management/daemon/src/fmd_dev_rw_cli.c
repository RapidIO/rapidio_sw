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
#ifdef WINDOWS
#include <io.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "riocp_pe_internal.h"
#include "fmd_dev_rw_cli.h"
#include "liblog.h"
#include "libcli.h"

#ifdef __cplusplus
extern "C" {
#endif

// Globals used by repeatable commands
static uint32_t store_address;
static uint32_t store_numbytes;
static uint32_t store_numacc;
static uint32_t store_data;

void aligningAddress(struct cli_env *env, uint32_t address)
{
	sprintf(env->output,
		"\nNote: Converting address 0x%08x to multiple of %d bytes\n",
		address, 4);
	logMsg(env);
};

void failedReading(struct cli_env *env, uint32_t address, uint32_t rc)
{
	sprintf(env->output, "\nFAILED reading, Address 0x%08x, rc 0x%08x\n",
		address, rc);
	logMsg(env);
};

void failedWrite(struct cli_env *env, uint32_t address, uint32_t data, uint32_t rc)
{
	sprintf(env->output,
		"\nFAILED writing, Address 0x%08x, Data 0x%08x, rc = 0x%08x\n",
	address, data, rc);
	logMsg(env);
};

int mport_read(riocp_pe_handle pe_h, uint32_t offset, uint32_t *data)
{
	uint32_t temp;
	int rc = 0;

        if (RIOCP_PE_IS_MPORT(pe_h))
                rc = rio_maint_read_local(pe_h->minfo->maint, offset,  &temp)?1:0;
        else {
		INFO("MTC READ: DID %x HC %X O %x\n", 
                        pe_h->destid, pe_h->hopcount, offset);
                rc = rio_maint_read_remote(pe_h->mport->minfo->maint,
			pe_h->destid, pe_h->hopcount, offset, &temp, 1)?1:0;
	}
	if (!rc)
		*data = temp;
	return rc;
};

int mport_write(riocp_pe_handle pe_h, uint32_t addr, uint32_t data)
{
	int rc = 0;

        if (RIOCP_PE_IS_MPORT(pe_h))
                rc = rio_maint_write_local(pe_h->minfo->maint, addr, data)?1:0;
        else
                rc = rio_maint_write_remote(pe_h->mport->minfo->maint,
			pe_h->destid, pe_h->hopcount, addr, &data, 1)?1:0;
	return rc;
};

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIRegReadCmd(struct cli_env *env, int argc, char **argv)
{
	int errorStat = 0;
	uint32_t address;
	uint32_t data, prevRead;
	uint32_t numReads = 1, i;
	int rc;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	if (argc) {
		address = getHex(argv[0], 0);
		if (argc > 1)
			numReads = getHex(argv[1], 1);

		if ((address % 4) != 0) {
			aligningAddress(env, address);
			address = address - (address % 4);
		}
		store_address = address;
		store_numacc = numReads;
	} else {
		address = store_address;
		numReads= store_numacc;
	};

	for (i = 0; i < numReads; i++) {
		rc = mport_read(pe_h, address, &data);

		if (rc) {
			failedReading(env, address, rc);
			goto exit;
		}
		if (!i) {
			sprintf(env->output, "\t0x%08x\t0x%08x\n",
				address, data);
			logMsg(env);
		} else if (data != prevRead) {
			sprintf(env->output,
				"\t0x%08x\t0x%08x (iteration 0x%x)*\n",
				address, data, i);
			logMsg(env);
		}
		prevRead = data;
	}
exit:
	return errorStat;
}

struct cli_cmd CLIRegRead = {
(char *)"read",
1,
1,
(char *)"read register",
(char *)"<address> {<numreads>}\n"
	"<address> : Register offset.  Must be 4 byte aligned.\n"
	"<repeat>  : Optional, number of times to read <address>\n"
	"            Default <repeat> is 1.\n",
CLIRegReadCmd,
ATTR_RPT
};

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIRegWriteCmd(struct cli_env *env, int argc, char **argv)

{
	int errorStat = 0;
	uint32_t address;
	uint32_t data;
	uint32_t rc;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	if (argc) {
		address = getHex(argv[0], 0);
		data    = getHex(argv[1], 0);
		if ((address % 4) != 0) {
			/*ensure that the address is a multiple of n bytes*/
			aligningAddress(env, address);
			address = address - (address % 4);
		};
		store_address = address;
		store_data    = data;
	} else {
		address = store_address;
		data = store_data ;
	};

	/* Command arguments are syntactically correct - do write */
	rc = mport_write(pe_h, address, data);
	if (0 != rc) {
		failedWrite(env, address, data, rc);
		goto exit;
	}

	/* read data back */
	rc = mport_read(pe_h, address, &data);
	if (0 != rc) {
		failedReading(env, address, rc);
		goto exit;
	} else {
		sprintf(env->output, "\nRead back %08x\n", data);
		logMsg(env);
	}

exit:
	return errorStat;
}

struct cli_cmd CLIRegWrite = {
(char *)"write",
1,
2,
(char *)"write register, then read back updated register value",
(char *)"<address> <data>\n"
	"Write <data> at <address> for current device.\n"
	"<address> must be 4 byte aligned.\n"
	"<data> is 4 bytes.\n",
CLIRegWriteCmd,
ATTR_RPT
};

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIRegReWriteCmd(struct cli_env *env, int argc, char **argv)
{
	int errorStat = 0;
	uint32_t address;
	uint32_t data;
	uint32_t repeat, i;
	uint32_t rc;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	if (argc) {
		address = getHex(argv[0], 0);
		data    = getHex(argv[1], 0);
		repeat  = getHex(argv[2], 0);
		if ((address % 4) != 0) {
			aligningAddress(env, address);
			address = address - (address % 4);
		};
		store_address = address;
		store_data = data;
		store_numacc = repeat;
	} else {
		address = store_address;
		data    = store_data;
		repeat  = store_numacc;
	};


	for (i = 0; i < repeat; i++) {
		rc = mport_write(pe_h, address, data);
		if (0 != rc) {
			failedWrite(env, address, data, rc);
			goto exit;
		};
	};

	rc = mport_read(pe_h, address, &data);
	if (0 != rc) {
		failedReading(env, address, rc);
		goto exit;
	} else {
		sprintf(env->output, "\nRead back 0x%08x\n", data);
		logMsg(env);
	}
exit:
	return errorStat;
}

struct cli_cmd CLIRegReWrite = {
(char *)"REWrite",
3,
3,
(char *)"write register repeatedly, then read back updated value",
(char *)"<address> <data> <repeat>\n"
"Write <data> at <address> for current device <repeat> times.\n"
	"<address> must be 4 byte aligned.\n"
	"<data> is 4 bytes.\n"
	"<repeat> can be up to 0xFFFFFFFF",
CLIRegReWriteCmd,
ATTR_RPT
};

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIRegWriteNoReadbackCmd(struct cli_env *env, int argc, char **argv)

{
	int errorStat = 0;
	uint32_t address;
	uint32_t data;
	uint32_t rc;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	if (argc) {
		address = getHex(argv[0], 0);
		data    = getHex(argv[1], 0);

		if ((address % 4) != 0) {
			/*ensure that the address is a multiple of n bytes*/
			aligningAddress(env, address);
			address = address - (address % 4);
		};
		store_address = address;
		store_data = data;
	} else {
		address = store_address;
		data = store_data;
	};

	/* Command arguments are syntactically correct - do write */
	rc = mport_write(pe_h, address, data);
	if (0 != rc) {
		failedWrite(env, address, data, rc);
		goto exit;
	} else {
		sprintf(env->output, "\nWrite successful\n");
		logMsg(env);
	}
exit:
	return errorStat;
}

struct cli_cmd CLIRegWriteNoReadback = {
(char *)"Write",
1,
2,
(char *)"write register",
(char *)"<address> <data>\n"
"Write <data> at <address> for current device\n"
"<address> must be 4 byte aligned.\n"
"<data> is 4 bytes.",
CLIRegWriteNoReadbackCmd,
ATTR_RPT
};

int expect(struct cli_env *env, int argc, char **argv, int inverse)
{
	int errorStat = 0;
	uint32_t address;
	uint32_t data, expdata;
	uint32_t rc;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	if (argc) {
		address = getHex(argv[0], 0);
		expdata = getHex(argv[1], 1);

		if ((address % 4) != 0) {
			aligningAddress(env, address);
			address = address - (address % 4);
		};
		store_address = address;
		store_data = expdata;
	} else {
		address = store_address;
		expdata = store_data;
	};

	rc = mport_read(pe_h, address, &data);
	if (0 != rc) {
		failedReading(env, address, rc);
		goto exit;
	};

	if (((data == expdata) && (!inverse)) ||
	    ((data != expdata) &&  (inverse))) {
		sprintf(env->output,
		"\nPASSED: Address: 0x%08x Data 0x%08x ExpData 0x%08x\n",
		address, data, expdata);
	} else {
		sprintf(env->output,
		"\nFAILED: Address: 0x%08x Data 0x%08x ExpData 0x%08x\n",
		address, data, expdata);
	}
	logMsg(env);
exit:
	return errorStat;
}

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIRegExpectNotCmd(struct cli_env *env, int argc, char **argv)
{
	return expect(env, argc, argv, 1);
}

struct cli_cmd CLIRegExpectNot = {
(char *)"expnot",
4,
2,
(char *)"check register does not match specified value",
(char *)"<address> <data>\n"
	"Read register at <address> on current device, compare to <data>\n"
	"Print an error message if value read is equal to <data>\n"
	"<address> must be 4 byte aligned.\n"
	"<data> is 4 bytes.",
CLIRegExpectNotCmd,
ATTR_RPT
};

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIRegExpectCmd(struct cli_env *env, int argc, char **argv)
{
	return expect(env, argc, argv, 0);
}

struct cli_cmd CLIRegExpect = {
(char *)"expect",
2,
2,
(char *)"check register matches expected value",
(char *)"<address> <data>\n"
	"Read register at <address> on current device, compare to <data>\n"
	"Print an error message if value read is not equal to <data>\n"
	"<address> must be 4 byte aligned.\n"
	"<data> is 4 bytes.",
CLIRegExpectCmd,
ATTR_RPT
};

/* If the structure or syntax of this command changes,
 * please update the Help structure following the procedure.
 */

int CLIRegDumpCmd(struct cli_env *env, int argc, char **argv)

{
	int errorStat = 0;
	uint32_t address, data;
	uint32_t numbytes;
	uint32_t i;
	uint32_t rc;
	static uint32_t store_address, store_numbytes;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	if (argc) {
		address  = getHex(argv[0], 0);
		numbytes = getHex(argv[1], 1);
	} else {
		/* in the special case of a continous dump command,
		 * this function is called with argc == 0
		 */
		address = store_address;
		numbytes = store_numbytes;
	}

	if ((address % 4) != 0) {
		/* ensure that the address is a multiple of n bytes */
		aligningAddress(env, address);
		address = address - (address % 4);
	}

	/* Dump columnar data for 16 bytes at a time.
	 * First get the output alinged for the entered address
	 */


	sprintf(env->output, "\nAddress  00____03 04____07 08____0B 0C____0F");
	logMsg(env);
	sprintf(env->output, "\n%8x", address & 0xFFFFFFF0);
	logMsg(env);
	for (i = 0; i < (address & 0xF); i += 4) {
		sprintf(env->output, "         ");
		logMsg(env);
	};
	for (i = 0; i < numbytes; i += 4) {
		rc = mport_read(pe_h, address + i, &data);
		if (0 != rc) {
			failedReading(env, address, rc);
			goto exit;
		}
		sprintf(env->output, " %08x", data);
		logMsg(env);
		if ((0xC == ((address + i) & 0xF)) &&
			((i + 4) < numbytes)) {
			sprintf(env->output, "\n%8x",
				(address+i+4) & 0xFFFFFFF0);
			logMsg(env);
		};
	};
	sprintf(env->output, "\n");
	logMsg(env);

	/* store data for continuous dump command */
	store_address = address + numbytes;
	store_numbytes =  numbytes;

exit:
	return errorStat;
}

struct cli_cmd CLIRegDump = {
(char *)"dump",
1,
2,
(char *)"display a block of memory/registers",
(char *)"<address> <numbytes>\n"
"Read 4 byte registers starting at <address> on current device\n"
	"<address> will be rounded down to 4 byte alignment.\n"
	"<numbytes> will be rounded up to 4 byte alignment.\n",
CLIRegDumpCmd,
ATTR_RPT
};

struct cli_cmd *reg_cmd_list[] = {
&CLIRegRead,
&CLIRegWrite,
&CLIRegReWrite,
&CLIRegWriteNoReadback,
&CLIRegExpect,
&CLIRegExpectNot,
&CLIRegDump
};

void fmd_bind_dev_rw_cmds(void)
{
	// Init globals used by repeatable commands
	store_address = 0;
	store_numbytes = 4;
	store_numacc = 1;
	store_data = 0;

	add_commands_to_cmd_db(sizeof(reg_cmd_list)/
			sizeof(struct cli_cmd *), reg_cmd_list);
};

#ifdef __cplusplus
}
#endif
