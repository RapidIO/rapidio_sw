/*
****************************************************************************
Copyright (c) 2016, Integrated Device Technology Inc.
Copyright (c) 2016, RapidIO Trade Association
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

/**
 * \file memops_demo.cc
 *
 * \brief Demonstrate RapidIO memory operations application interface for DMA.  
 * Performs a DMA write and read transaction to a remote memory address 
 * using either the
 * libmport, Shared DMA User Mode Driver or Unique DMA User Mode Driver.
 * Synchronous, asynchronous, and fire-and-forget synchronization are supported.
 *
 * No Log output: ./memops_demo (sync) (driver) (destid) (addr)
 *
 * Log output: ./memops_demo_log (sync) (driver) (destid) (addr)
 *
 * - sync is one of:
 *   - -s sync transaction, wait forever until the transaction is complete
 *   - -a async transaction, send transaction and wait separately for completion
 *   - -f fire and forget transaction, do not check for completion
 *   - Note: default is -s (sync)
 * - method is one of
 *   - -M mport : libmport kernel mode driver
 *   - -S Shared UMD : DMA User Mode Driver channel can be shared between
 *   		 processes
 *   - -U Unique UMD : DMA User Mode Driver channel is dedicated to one process
 * - destid - the destination ID of the node supporting the target memory 
 * - addr - RapidIO address of an inbound window on the target device
 *
 * The hardware channel used by the user mode driver is controlled by the
 * UMD_CHAN environment variable.  Set the environment variable 
 * before executing memops_demo(_log):
 *
 *	 export UMD_CHAN=5
 * 
 * To demonstrate the Shared DMA user mode driver, UMD_CHAN must be the same
 * as the channel used to start the Shared DMA daemon.  Usually, the channel
 * number is 5.
 *
 * To demonstrate the Unique DMA user mode driver, UMD_CHAN must be different 
 * from the channel used to start the Shared DMA daemon, and must not match
 * any in use DMA driver.  Typically, channel 6 (UMD_CHAN=6) is available for
 * the demo.
 *
 * The shared memory user mode driver uses the UMDD_LIB environment variable 
 * that controls the path to the driver library.  Set the environment variable
 * before executing memops_demo using the following, assuming that
 * memops_demo is executed from the rapidio_sw/memops directory:
 *
 *	 export UMDD_LIB=../umdd_tsi721/libUMDd.so.0.4
 *
 * When using memops_demo_log specify the logged library:
 *
 *	 export UMDD_LIB=../umdd_tsi721/libUMDd_log.so.0.4
 *
 * Note that memops_sample requires an inbound memory window on another node
 * to act as a target for read and write transactions.  The inbound memory 
 * window can be created by using the start_target script in the goodput
 * utility.  For more information, refer to documentation of the goodput
 * utility.
 *
 */

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <getopt.h>

#ifdef RDMA_LL
	#include "liblog.h"
#endif

#include "memops.h"
#include "memops_umd.h"

extern "C" bool DMAChannelSHM_has_logging();

int timeout = 1000; // miliseconds

void usage_and_exit(const char* name)
{
	fprintf(stderr, " Usage:\n"
	" ./memops_demo (sync) (driver) -d destid -t addr\n"
	" - sync is at least one of:\n"
	"   -s sync transaction, wait forever until the transaction\n"
	"      is complete\n"
	"   -a async transaction, send transaction and wait separately\n"
	"      for completion\n"
	"   -f fire and forget transaction, do not check for completion\n"
	"  Default is -A\n"
	"  - Note: default is -A (sync)\n"
	" - driver is at least one of:\n"
	"   - -M libmport kernel mode driver\n"
	"   - -S DMA User Mode Driver, share DMA channels between processes\n"
	"   - -U DMA User Mode Driver, DMA channel dedicated to one process\n"
	"   Default is -M\n"
	" - dest - the destination ID of the node with the target memory\n"
	"          If this parameter is not entered, memops_demo fails.\n"
	" - addr - RapidIO address of an inbound window on the target device\n"
	"          default is 0x200000000\n");
	exit(0);
}

MEMOPSAccess_t met[] = {MEMOPS_MPORT, MEMOPS_UMDD, MEMOPS_UMD};

const char* met_str[] = {"mport", "UMDd/SHM", "UMD"};

/** \brief Demonstrate combined memops interface for DMA read/write
 * 
 * Note: Using libUMDd_log with non-logged version(memops_test)
 * will cause a sema hang in rdma_log.
 *
 * The demonstration performs the following steps:
 *
 */

int main(int argc, char* argv[])
{
	const char* sync_str = "SYNC";
	enum riomp_dma_directio_transfer_sync sync = RIO_DIRECTIO_TRANSFER_SYNC;
	uint16_t did = 0xFFFF;
	uint64_t rio_addr = 0x200000000;
	int option;
	int m = 0;
	static const struct option options[] = {
		{ "destid", required_argument, NULL, 'd' },
		{ "addr"  , required_argument, NULL, 'a' },
		{ "sync"  , no_argument, NULL, 's' },
		{ "async" , no_argument, NULL, 'a' },
		{ "faf"   , no_argument, NULL, 'f' },
		{ "mport" , no_argument, NULL, 'M' },
		{ "UMD_Sh", no_argument, NULL, 'M' },
		{ "UMD"   , no_argument, NULL, 'U' },
		{ "help",   no_argument, NULL, 'h' },
		{ }
	};

	if (argc < 4)
		usage_and_exit(argv[0]);


	int ret = 0;
	int n = 1;

	/** - Parse parameters */
        while (1) {
                option = getopt_long_only(argc, argv,
                                "hsafMSUd:A:", options, NULL);
                if (option == -1)
                        break;
                switch (option) {
		case 's': sync = RIO_DIRECTIO_TRANSFER_SYNC;
			timeout = 0;
			sync_str = "SYNC(inft)";
			break;
		case 'a': sync = RIO_DIRECTIO_TRANSFER_ASYNC;
			sync_str = "ASYNC(tmout)";
			break;
		case 'f': sync = RIO_DIRECTIO_TRANSFER_FAF;
			sync_str = "FAF";
			break;
                case 'M':
                        m = 0;
                        break;
                case 'S':
                        m = 1;
                        break;
                case 'U':
                        m = 2;
                        break;
                case 'A':
                        rio_addr = strtol(optarg, NULL, 16);
                        break;
                case 'd':
                        did = strtol(optarg, NULL, 0);
                        break;
                case 'h':
			usage_and_exit(argv[0]);
                        break;
		default: fprintf(stderr, "Invalid option %s\n", argv[n]);
			usage_and_exit(argv[0]);
			break;
		}
	}

	if (0xFFFF == did)
		usage_and_exit(argv[0]);
	
#ifdef RDMA_LL
	rdma_log_init("memops_demo_log.txt", 1);
#else
	if (DMAChannelSHM_has_logging()) {
		fprintf(stderr, "Selected version of UMDD_LIB (%s)"
		" has logging compiled ON and is called in a logging"
		" OFF binary!\n", getenv("UMDD_LIB"));
		return 69;
	}
#endif

	/** - Get UMD channel from UMD_CHAN environment variable */
	int chan = 6;
	if (getenv("UMD_CHAN") != NULL)
		chan = atoi(getenv("UMD_CHAN"));

	/** - Create memory operation object */
	RIOMemOpsIntf* mops = RIOMemOps_classFactory(met[m], 0, chan);

	printf("HW access method=%s %s destid=%u rio_addr=0x%" PRIx64 " [chan=%d]\n",
		met_str[m], sync_str, did, rio_addr, chan);

	/** - Set up the hardware, and create hardware management thread */
	if (met[m] == MEMOPS_UMD) {
		RIOMemOpsUMD* mops_umd = dynamic_cast<RIOMemOpsUMD*>(mops);
		bool r = mops_umd->setup_channel(0x100, 0x400);
		assert(r);
		r = mops_umd->start_fifo_thr(-1);
		assert(r);
	}

	/** - Create an inbound window, not otherwise used in the test */
	DmaMem_t ibmem;
	memset(&ibmem, 0, sizeof(ibmem));

	ibmem.rio_address = RIO_ANY_ADDR;
	mops->alloc_dmawin(ibmem, 40960);
	printf("IBWin RIO addr @0x%lx size 0x%x\n",
		ibmem.win_handle, ibmem.win_size);

	const int TR_SZ = 256;

	/** - Write data to the target address on the selected destid */
	MEMOPSRequest_t req; memset(&req, 0, sizeof(req));
	req.destid = did;
	req.bcount = TR_SZ;
	req.raddr.lsb64 = rio_addr;
	req.mem.rio_address = RIO_ANY_ADDR;
	mops->alloc_dmawin(req.mem, 40960);
	req.mem.offset = TR_SZ;
	req.sync = sync;
	req.wr_mode = RIO_DIRECTIO_TYPE_NWRITE_R;

	uint8_t* p = (uint8_t*)req.mem.win_ptr;
	p[TR_SZ] = 0xdb;
	p[TR_SZ+1] = 0xae;

	if (! mops->nwrite_mem(req)) {
		int abort = mops->getAbortReason();
		fprintf(stderr, "NWRITE_R failed with reason %d (%s)\n",
			abort, mops->abortReasonToStr(abort));
		ret = 1;
		goto done;
	}
 
	/** - Check that the DMA engine is error free, for Fire-and-forget */
	if (mops->canRestart() && mops->checkAbort()) {
		int abort = mops->getAbortReason();
		fprintf(stderr, "NWRITE_R ABORTed with reason %d (%s)\n",
			abort, mops->abortReasonToStr(abort));
		mops->restartChannel();
		ret = 41;
		goto done;
	}

	/** - Wait until an asynchronous write transaction has completed */
	if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
		bool r = mops->wait_async(req, timeout);
		if (!r) {
			int abort = mops->getAbortReason();
			fprintf(stderr, "NWRITE_R async wait failed after %dms"
				" with reason %d (%s)\n",
				timeout, abort, mops->abortReasonToStr(abort));
			ret = 42;
			goto done;
		}
	}

	/** - Read back the data that was written */
	req.raddr.lsb64 = rio_addr + 256; 
	req.mem.offset = 0;

	if (! mops->nread_mem(req)) {
		int abort = mops->getAbortReason();
		fprintf(stderr, "NREAD failed with reason %d (%s)\n",
			abort, mops->abortReasonToStr(abort));
		ret = 1;
		goto done;
	} 

	/** - Check that the DMA engine is error-free */
	if (mops->canRestart() && mops->checkAbort()) {
		int abort = mops->getAbortReason();
		fprintf(stderr, "NREAD ABORTed with reason %d (%s)\n",
			abort, mops->abortReasonToStr(abort));
		mops->restartChannel();
		ret = 41;
		goto done;
	}

	/** - Wait until an asynchronous read transaction has completed */
	if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
		bool r = mops->wait_async(req, timeout);
		if (!r) {
			int abort = mops->getAbortReason();
			fprintf(stderr, "NREAD async wait failed after %dms "
				"with reason %d (%s)\n",
				timeout, abort, mops->abortReasonToStr(abort));
			ret = 42;
			goto done;
		}
	}

	/** - Display the memory values, one byte at a time */
	printf("Mem-in:\n");
	for (int j = 0; j < 16; j++) 
		printf("%02x ", j);
	printf("\n");
	for (int i = 0; i < TR_SZ; i++) {
		printf("%02x ", p[i]);
		if (0 == ((i+1) % 16))
			printf("\n");
	}
	printf("\n");

done:
	delete mops;

#ifdef RDMA_LL
	rdma_log_close();
#endif

	return ret;
}
