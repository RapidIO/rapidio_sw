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

//#include "riocp_pe_internal.h"
#include "fmd.h"
#include "fmd_dev_rw_cli.h"
#include "liblog.h"
#include "libcli.h"
#include "riocp_pe_internal.h"
#include "pe_mpdrv_private.h"

#ifdef __cplusplus
extern "C" {
#endif

// Globals used by repeatable commands
static uint32_t store_address;
static uint32_t store_numbytes;
static uint32_t store_numacc;
static uint32_t store_data;

static uint32_t mstore_address;
static uint32_t mstore_numbytes;
static uint32_t mstore_numacc;
static uint32_t mstore_data;
static uint32_t mstore_did;
static uint32_t mstore_hc;

void aligningAddress(struct cli_env *env, uint32_t address)
{
	sprintf(env->output,
		"\nNote: Converting address 0x%08x to multiple of %d bytes\n",
		address, 4);
	logMsg(env);
};

void failedReading(struct cli_env *env, uint32_t address, uint32_t rc)
{
	sprintf(env->output,
		"\nFAILED reading, Address 0x%08x, rc 0x%08x\n", address, rc);
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
	int rc;
	uint32_t temp_data;

	rc = pe_h->mport->minfo->reg_acc.reg_rd(pe_h, offset, &temp_data);

	if (!rc)
		*data = temp_data;
	return rc;
};

int mport_write(riocp_pe_handle pe_h, uint32_t offset, uint32_t data)
{
	int rc = 0;
	rc = pe_h->mport->minfo->reg_acc.reg_wr(pe_h, offset, data);

	return rc;
};

int CLIDevSelCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t comptag, pe_ct;
	riocp_pe_handle *pes = NULL;
	size_t pes_count, i;
	int rc;
	struct riocp_pe_capabilities caps;
	const char *dev_name, *vend_name;
	struct cfg_dev cfg_dev;
	const char *no_name = "NO_NAME";

	rc = riocp_mport_get_pe_list(mport_pe, &pes_count, &pes);
	if (rc) {
		sprintf(env->output, "\nCould not get PE list\n");
		logMsg(env);
		goto exit;
	}

	if (argc) {
		for (i = 0; i < pes_count; i++) {
			if (cfg_find_dev_by_ct(pes[i]->comptag, &cfg_dev)) {
				continue;
			};
			if (!strcmp(cfg_dev.name, argv[0]) &&
				(strlen(cfg_dev.name) == strlen(argv[0]))) {
				env->h = pes[i];
				set_prompt(env);
				sprintf(env->output, 
					"\nFound device named \"%s\"\n",
					argv[0]);
				logMsg(env);
				goto exit;
			};
		};
		
		comptag = getHex(argv[0], 0);

		for (i = 0; i < pes_count; i++) {
			rc = riocp_pe_get_comptag(pes[i], &pe_ct);
			if (rc) {
				sprintf(env->output,
					"\nFailed reading CT: %d\n", rc);
				logMsg(env);
				goto exit;
			}
			if (comptag == pe_ct) {
				env->h = pes[i];
				set_prompt(env);
				sprintf(env->output, 
					"\nFound device for CT 0x%08x\n",
					pe_ct);
				logMsg(env);
				goto exit;
			};
		};
	};

	if (NULL != env->h) {
		rc = riocp_pe_get_comptag((riocp_pe_handle)env->h, &comptag);
		if (rc) {
			sprintf(env->output, "\nFailed reading CT: %d\n", rc);
			logMsg(env);
			goto exit;
		}
	}

	if (!pes_count) {
		sprintf(env->output, "\nNo PEs discovered!\n");
		logMsg(env);
		goto exit;
	}
	
	sprintf(env->output,
	"\n  CompTag -->Sysfs Name<-- ----------->> Vendor <<-------------------  Device\n");
	logMsg(env);
	for (i = 0; i < pes_count; i++) {
		rc = riocp_pe_get_comptag(pes[i], &pe_ct);
		if (rc) {
			sprintf(env->output, "\nFailed reading CT: %d\n", rc);
			logMsg(env);
			goto exit;
		}
		rc = riocp_pe_get_capabilities(pes[i], &caps);
		if (rc) {
			sprintf(env->output,
				"\nFailed reading capabilities: %d\n", rc);
			logMsg(env);
			goto exit;
		}

		if (cfg_find_dev_by_ct(pe_ct, &cfg_dev))
			cfg_dev.name = no_name;
		dev_name = riocp_pe_get_device_name(pes[i]);
		vend_name = riocp_pe_get_vendor_name(pes[i]);

		sprintf(env->output, "%s%08x %16s %42s %10s\n",
			(pe_ct == comptag)?"*":" ",
			pe_ct, cfg_dev.name, vend_name, dev_name);
		logMsg(env);
	}
exit:
	rc = riocp_mport_free_pe_list(&pes);
	if (rc) {
		sprintf(env->output, "\nFailed freeing PE list %d\n", rc);
		logMsg(env);
	}
	return 0;
}

struct cli_cmd CLIDevSel = {
(char *)"devsel",
3,
0,
(char *)"display available devices or select a device",
(char *)"{<comptag>}\n"
	"<comptag> : Optional parameter, used to select a device as\n"
	"            the target for register reads and writes.\n"
	"            Can be component tag value of device name.\n",
CLIDevSelCmd,
ATTR_RPT
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

int CLIMRegReadCmd(struct cli_env *env, int argc, char **argv)
{
	int errorStat = 0;
	uint32_t address;
	uint32_t data, prevRead;
	uint32_t numReads = 1, i;
	uint32_t did;
	uint32_t hc;
	int rc;

	if (argc) {
		address = getHex(argv[0], 0);
		if(argc > 2) {
			did = getHex(argv[1], 0);
			hc = getHex(argv[2], 0);
		} else {
			did = mstore_did;
			hc = mstore_hc;
		};
		
		if(argc > 3)
			numReads = getHex(argv[3], 1);

		if ((address % 4) != 0) {
			aligningAddress(env, address);
			address = address - (address % 4);
		}
		mstore_address = address;
		mstore_numacc = numReads;
		mstore_did = did;
		mstore_hc = hc;
	} else {
		address = mstore_address;
		numReads= mstore_numacc;
		did = mstore_did;
		hc = mstore_hc;
	};

	for (i = 0; i < numReads; i++) {
		rc = pe_mpsw_rw_driver.raw_reg_rd((riocp_pe_handle)env->h,
			did, hc, address, &data);

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

struct cli_cmd CLIMRegRead = {
(char *)"mread",
2,
1,
(char *)"maintenance read register",
(char *)"<address> {<devid> <hc> {<numreads>}}\n"
	"<address> : Register offset.  Must be 4 byte aligned.\n"
	"<devid>   : Optional, device ID to read.\n"
	"            If not present, use last entered devid.\n"
	"<hc>      : Hop count. Specify FF to access mport.\n"
	"            If not present, use last entered hc.\n"
	"<repeat>  : Optional, number of times to read <address>\n"
	"            Default <repeat> is 1.\n",
CLIMRegReadCmd,
ATTR_RPT
};

int CLIMRegWriteCmd(struct cli_env *env, int argc, char **argv)

{
	int errorStat = 0;
	uint32_t address;
	uint32_t did;
	uint32_t hc;
	uint32_t data;
	uint32_t rc;

	if (argc) {
		address = getHex(argv[0], 0);
		data    = getHex(argv[1], 0);
		if (argc > 3) {
			did = getHex(argv[2], 0);
			hc = getHex(argv[3], 0);
		} else {
			did = mstore_did;
			hc = mstore_hc;
		};
		if ((address % 4) != 0) {
			/*ensure that the address is a multiple of n bytes*/
			aligningAddress(env, address);
			address = address - (address % 4);
		};
		mstore_address = address;
		mstore_data    = data;
		mstore_did     = did;
		mstore_hc    = hc;
	} else {
		address = mstore_address;
		data = mstore_data;
		did = mstore_did;
		hc = mstore_hc;
	};

	/* Command arguments are syntactically correct - do write */

	rc = pe_mpsw_rw_driver.raw_reg_wr((riocp_pe_handle)env->h,
		did, hc, address, data);

	if (0 != rc) {
		failedWrite(env, address, data, rc);
		goto exit;
	}

	/* read data back */
	rc = pe_mpsw_rw_driver.raw_reg_rd((riocp_pe_handle)env->h,
		did, hc, address, &data);

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

struct cli_cmd CLIMRegWrite = {
(char *)"mwrite",
2,
2,
(char *)"write register, then read back updated register value",
(char *)"<address> <data> {<devid> <hc>}\n"
	"Write <data> at <address> for device <devid> at <hc> hops away.\n"
	"<address>: must be 4 byte aligned.\n"
	"<data>   : 4 byte value to write.\n"
	"<did>    : device ID to access.\n"
	"           If not present, use last did entered.\n"
	"<hc>     : Hop count.  Use FF to access local mport registers.\n"
	"           If not present, use last did entered.\n",
CLIMRegWriteCmd,
ATTR_RPT
};

int CLICountReadCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	uint32_t cntr;
        uint8_t srch_i;
	struct mpsw_drv_private_data *priv = NULL;
	DAR_DEV_INFO_t *dev_h = NULL;
	idt_sc_read_ctrs_in_t  sc_in;
	idt_sc_read_ctrs_out_t sc_out;
	riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
	dev_h = &priv->dev_h;

	if (NULL == pe_h) {
		sprintf(env->output, "\nNo Device Selected...\n");
		logMsg(env);
		goto exit;
	};

	sc_in.ptl.num_ports = RIO_ALL_PORTS;
	sc_in.dev_ctrs = &priv->st.sc_dev;
	for (srch_i = 0; srch_i < sc_in.dev_ctrs->valid_p_ctrs; srch_i++) {
                for (cntr = 0; cntr < 8; cntr++) {
                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = true;
                        switch (cntr) {
                                case 0:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pkt;
                                break;
                                case 1:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pkt;
                                break;
                                case 2:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pcntr;
                                break;
                                case 3:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pcntr;
                                break;
                                case 4:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_ttl_pcntr;
                                break;
                                case 5:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_retries;
                                break;
                                case 6:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pna;
                                break;
                                case 7:
                                        sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pkt_drop;
                                break;
                        }
                }
        }
	rc = idt_sc_read_ctrs(dev_h, &sc_in, &sc_out);
	if (RIO_SUCCESS != rc)
		goto exit;

exit:
	return 0;
};

struct cli_cmd CLICountRead = {
(char *)"rcnt",
1,
0,
(char *)"Read the count registers for a port",
(char *)"rcnt \n",
CLICountReadCmd,
ATTR_NONE
};

#define RXS_NUM_CTRS        8

int CLICountDisplayCmd(struct cli_env *env, int argc, char **argv)
{
        uint32_t rc;
        struct mpsw_drv_private_data *priv = NULL;
        DAR_DEV_INFO_t *dev_h = NULL;
        idt_sc_read_ctrs_in_t  sc_in;
        idt_sc_read_ctrs_out_t sc_out;
	idt_sc_init_dev_ctrs_in_t sc_init_in;
	idt_sc_init_dev_ctrs_out_t sc_init_out;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);
	uint32_t val_p, cntr;
	uint8_t srch_i;
	char padding[5];
	int idx = 0, j = 0;
        char temp[14], *type;
	char *tx;
        bool first = false;

        priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
        dev_h = &priv->dev_h;

        if (NULL == pe_h) {
                sprintf(env->output, "\nNo Device Selected...\n");
                logMsg(env);
                goto exit;
 	};

        sc_in.ptl.num_ports = RIO_ALL_PORTS;
        sc_in.dev_ctrs = &priv->st.sc_dev;

	if (RXS2448_RIO_DEVICE_ID == ((dev_h->devID & 0xffff0000) >> 16) || 
		RXS1632_RIO_DEVICE_ID == ((dev_h->devID & 0xffff0000) >> 16))
	{
		printf("RXS CONTERS");

		for (srch_i = 0; srch_i < sc_in.dev_ctrs->valid_p_ctrs; srch_i++) {
                	for (cntr = 0; cntr < RXS_NUM_CTRS; cntr++) {
				sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].tx = true;
				switch (cntr) {
					case 0:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pkt;
					break;
					case 1:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pkt;
                                	break;
					case 2:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_pcntr;
                               		break;
					case 3:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_fab_pcntr;
                                	break;
					case 4:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_rio_ttl_pcntr;
                                	break;
					case 5:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_retries;
                                	break;
					case 6:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pna;
                                	break;
					case 7:
						sc_in.dev_ctrs->p_ctrs[srch_i].ctrs[cntr].sc = idt_sc_pkt_drop;
                                	break;
				}
                	}
        	}
	}
	else 
	{
		sc_init_in.ptl.num_ports = RIO_ALL_PORTS;
	        sc_init_in.dev_ctrs = &priv->st.sc_dev;
		priv->st.sc_dev.num_p_ctrs   = IDT_MAX_PORTS;
		priv->st.sc_dev.valid_p_ctrs = 0;
		priv->st.sc_dev.p_ctrs       = priv->st.sc;
		rc = idt_sc_init_dev_ctrs(dev_h, &sc_init_in, &sc_init_out);
		if (RIO_SUCCESS != rc)
			goto exit;
		sc_in = *((idt_sc_read_ctrs_in_t *)&sc_init_in);
	}

        rc = idt_sc_read_ctrs(dev_h, &sc_in, &sc_out);
        if (RIO_SUCCESS != rc)
                goto exit;

	printf("\n  port  Counter        Type                     LastRead               Total   \n ");
	for (val_p = 0; val_p < sc_in.dev_ctrs->valid_p_ctrs; ++val_p)
	{
		printf("\n  %2d    ",sc_in.dev_ctrs->p_ctrs[val_p].pnum);
		for (cntr = 0; cntr < sc_in.dev_ctrs->p_ctrs[val_p].ctrs_cnt; ++cntr) {
			strcpy(padding, cntr == 0 ? "  " : "          ");
			type = SC_NAME(sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].sc);
			tx = (sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].tx) ? (char *)"TX" : (char *)"RX";

       			memset(temp, 0x20, sizeof(temp));
		        while (type[idx]) {
				if (type[idx] != 0x20) {
                        		temp[j] = type[idx];
                        		j++;
                		}
                		else if (!first) {
                        		first = true;
					temp[j++] = 0x20;
                        		temp[j++] = tx[0];
                        		temp[j++] = tx[1];
                        		temp[j++] = 0x20;

                		}
                		idx++;
        		}
        		temp[13] = '\0';

			printf("%s%2d        %s            %8x             %8llx \n", padding, cntr, temp, 
                                sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].last_inc,
                                sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].total );
			memset(temp, 0x20, sizeof(temp));
			type = NULL;
			tx = NULL;
			idx = 0;
			j = 0;
			first = false;
		}
	}

	printf("\n");

exit:
        return 0;
};

struct cli_cmd CLICountDisplay = {
(char *)"cnt",
1,
0,
(char *)"Display the count registers for a port",
(char *)"cnt \n",
CLICountDisplayCmd,
ATTR_NONE
};

int CLICountCfgCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t rc;
	uint32_t val_p, cntr;
	bool tx = true ;
	bool is_print = false;
	char padding[5];
        struct mpsw_drv_private_data *priv = NULL;
        DAR_DEV_INFO_t *dev_h = NULL;
	idt_sc_cfg_rxs_ctr_in_t  sc_in;
        idt_sc_cfg_rxs_ctr_out_t sc_out;
	idt_sc_cfg_cps_ctrs_in_t sc_cps_in;
	idt_sc_cfg_cps_ctrs_out_t sc_cps_out;
	idt_sc_init_dev_ctrs_in_t sc_init_in;
        idt_sc_init_dev_ctrs_out_t sc_init_out;
        riocp_pe_handle pe_h = (riocp_pe_handle)(env->h);

	/*if (argc) {
		if (argv[0] != NULL)
			is_print = (bool)atoi(argv[0]);
	}*/

        priv = (struct mpsw_drv_private_data *)(pe_h->private_data);
        dev_h = &priv->dev_h;

        if (NULL == pe_h) {
                sprintf(env->output, "\nNo Device Selected...\n");
                logMsg(env);
                goto exit;
        };

	if (RXS2448_RIO_DEVICE_ID == ((dev_h->devID & 0xffff0000) >> 16) ||
                RXS1632_RIO_DEVICE_ID == ((dev_h->devID & 0xffff0000) >> 16))
        {
                printf("RXS CONTERS");

		for (sc_in.ctr_idx = 0; sc_in.ctr_idx < RXS_NUM_CTRS; ++sc_in.ctr_idx) {
			sc_in.ptl.num_ports = RIO_ALL_PORTS;
       			sc_in.dev_ctrs = &priv->st.sc_dev; 
        		sc_in.tx = tx;
        		sc_in.ctr_en = 0x80000000;
        		sc_in.prio_mask = 0xff;

			switch (sc_in.ctr_idx) {
                        	case 0:
                                	sc_in.ctr_type = idt_sc_rio_pkt;
                        	break;
                        	case 1:
                                	sc_in.ctr_type = idt_sc_rio_pkt;
					sc_in.tx = !tx;
                        	break;
                        	case 2:
                                	sc_in.ctr_type = idt_sc_rio_pcntr;
                        	break;
                        	case 3:
                                	sc_in.ctr_type = idt_sc_rio_pcntr;
					sc_in.tx = !tx;
                        	break;
                        	case 4:
                                	sc_in.ctr_type = idt_sc_retries;
                        	break;
                        	case 5:
                                	sc_in.ctr_type = idt_sc_retries;
					sc_in.tx = !tx;
                        	break;
                        	case 6:
                                	sc_in.ctr_type = idt_sc_rio_ttl_pcntr;
                        	break;
                        	case 7:
                                	sc_in.ctr_type = idt_sc_pna;
                        	break;
                 	}

			rc = idt_sc_cfg_rxs_ctr(dev_h, &sc_in, &sc_out);
			if (RIO_SUCCESS != rc)
                		goto exit;
		}
	}
	else
	{
		sc_init_in.ptl.num_ports = RIO_ALL_PORTS;
        	sc_init_in.dev_ctrs = &priv->st.sc_dev;
        	priv->st.sc_dev.num_p_ctrs   = IDT_MAX_PORTS;
        	priv->st.sc_dev.valid_p_ctrs = 0;
        	priv->st.sc_dev.p_ctrs       = priv->st.sc;
        	rc = idt_sc_init_dev_ctrs(dev_h, &sc_init_in, &sc_init_out);
        	if (RIO_SUCCESS != rc)
                	goto exit;

		sc_cps_in.enable_ctrs = true;
		sc_cps_in.ptl = sc_init_in.ptl;
		sc_cps_in.dev_ctrs = sc_init_in.dev_ctrs;
		rc = idt_sc_cfg_cps_ctrs(dev_h, &sc_cps_in, &sc_cps_out);
                if (RIO_SUCCESS != rc)
        		goto exit;
	}

	if (!is_print)
		goto exit;

	printf("\n  port  Counter         Type            LastRead        Total   \n ");
        for (val_p = 0; val_p < sc_in.dev_ctrs->valid_p_ctrs; ++val_p)
        {
                strcpy(padding, sc_in.dev_ctrs->p_ctrs[val_p].pnum < 10 ? "     " : "    ");
                printf("\n  %d%s",sc_in.dev_ctrs->p_ctrs[val_p].pnum, padding);
                for (cntr = 0; cntr < sc_in.dev_ctrs->p_ctrs[val_p].ctrs_cnt; ++cntr) {
                        strcpy(padding, cntr == 0 ? "   " : "           ");
                        printf("%s%d        %s %s           %d             %lld \n", padding, cntr, SC_NAME(sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].sc),
                                (sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].tx) ? "TX" : "RX",
                                sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].last_inc,
                                sc_in.dev_ctrs->p_ctrs[val_p].ctrs[cntr].total );
                }
        }

exit:
        return 0;
};

struct cli_cmd CLICountCfg = {
(char *)"cfgcnt",
1,
0,
(char *)"Configure the count registers for a port",
(char *)"cfgcnt \n",
CLICountCfgCmd,
ATTR_NONE
};

struct cli_cmd *reg_cmd_list[] = {
&CLIRegRead,
&CLIRegWrite,
&CLIRegReWrite,
&CLIRegWriteNoReadback,
&CLIRegExpect,
&CLIRegExpectNot,
&CLIRegDump,
&CLIMRegRead,
&CLIMRegWrite,
&CLIDevSel,
&CLICountRead,
&CLICountDisplay,
&CLICountCfg
};

void fmd_bind_dev_rw_cmds(void)
{
	// Init globals used by repeatable commands
	store_address = 0;
	store_numbytes = 4;
	store_numacc = 1;
	store_data = 0;

	mstore_address = 0;
	mstore_did = 0;
	mstore_hc = 0xFF;
	mstore_numbytes = 4;
	mstore_numacc = 1;
	mstore_data = 0;

	add_commands_to_cmd_db(sizeof(reg_cmd_list)/
			sizeof(struct cli_cmd *), reg_cmd_list);
};

#ifdef __cplusplus
}
#endif
