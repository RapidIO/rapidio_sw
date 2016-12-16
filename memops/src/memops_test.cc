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

#include <stdio.h>

#ifdef RDMA_LL
  #include "liblog.h"
#endif

#include "tok_parse.h"
#include "memops.h"
#include "memops_umd.h"

extern "C" bool DMAChannelSHM_has_logging();

int timeout = 1000; // miliseconds

void usage(const char* name)
{
  fprintf(stderr, "usage: %s [-A|-a|-F] -m<method> <destid> <rioaddr>\n" \
                  "\t\t-A async transaction, blocking forever\n" \
                  "\t\t-a async transaction, blocking %dms\n" \
                  "\t\t-F faf transaction\n" \
                  "\t\tNote: default is sync\n" \
                  "\t\tMethod: m=mport, s=UMDd/SHM, u=UMD\n", name, timeout);
  exit(0);
}

MEMOPSAccess_t met[] = { MEMOPS_MPORT, MEMOPS_UMDD, MEMOPS_UMD };

const char* met_str[] = { "mport", "UMDd/SHM", "UMD" };

int main(int argc, char* argv[])
{
  if (argc < 4) usage(argv[0]);

 /* UMD and UMDd/SHM require a HW channel set in the shell env like so
  *   export UMD_CHAN=5
  *
  * UMDd/SHM needs access to a .so, provded that this program is executed in ~/rapidio_sw/memops
  *   export UMDD_LIB=../umdd_tsi721/libUMDd.so.0.4
  *
  * Iff using logged version of this program (memops_test_log) then use the logged version:
  *   export UMDD_LIB=../umdd_tsi721/libUMDd_log.so.0.4
  *
  * Note: Using libUMDd_log with non-logged version(memops_test) will cause a sema hang in rdma_log.
  */

  const char* sync_str = "SYNC";
  enum riomp_dma_directio_transfer_sync sync = RIO_DIRECTIO_TRANSFER_SYNC;

  int ret = 0;
  int n = 1;

  if (argv[n][0] == '-') {
    if (argc < 5 || strlen(argv[n]) < 2) {
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
    }

    switch(argv[n][1]) {
      case 'A': sync = RIO_DIRECTIO_TRANSFER_ASYNC; timeout = 0; sync_str = "ASYNC(inft)"; break;
      case 'a': sync = RIO_DIRECTIO_TRANSFER_ASYNC; sync_str = "ASYNC(tmout)"; break;
      case 'F': sync = RIO_DIRECTIO_TRANSFER_FAF; sync_str = "FAF"; break;
      default: fprintf(stderr, "Invalid option %s\n", argv[n]);
               usage(argv[0]);
               exit(EXIT_FAILURE);
    }
    n++;
  }

  int m = -1;
  if (argv[n][0] == '-') {
    if (strlen(argv[n]) < 3 || argv[n][1] != 'm') {
            fprintf(stderr, "Invalid option %s\n", argv[n]);
            usage(argv[0]);
            exit(EXIT_FAILURE);
    }
    switch(argv[n][2]) {
      case 'm': m = 0; break;
      case 's': m = 1; break;
      case 'u': m = 2; break;
      default: fprintf(stderr, "Invalid HW access method %s\n", argv[n]);
               usage(argv[0]);
               exit(EXIT_FAILURE);
    }
    n++;
  }

  if (-1 == m) {
	  usage(argv[0]);
	  exit(EXIT_FAILURE);
  }

  uint32_t did;
  if (tok_parse_did(argv[n++], &did, 0)) {
	  fprintf(stderr, TOK_ERR_DID_MSG_FMT);
	  usage(argv[0]);
	  exit(EXIT_FAILURE);
  }

  uint64_t rio_addr = 0;
  if (tok_parse_ull(argv[n++], &rio_addr, 0)) {
	  fprintf(stderr, TOK_ERR_ULL_HEX_MSG_FMT, "rio address");
	  usage(argv[0]);
	  exit(EXIT_FAILURE);
  }

#ifdef RDMA_LL
  rdma_log_init("memops_test_log.txt", 1);
#else
  if (DMAChannelSHM_has_logging()) {
    fprintf(stderr, "Selected version of UMDD_LIB (%s) has logging compiled ON and is called in a logging OFF binary!\n", getenv("UMDD_LIB"));
    exit(EXIT_FAILURE);
  }
#endif

  uint16_t chan = 7;
  char *env_var = getenv("UMD_CHAN");
  if (env_var != NULL) {
	  if (tok_parse_ulong(env_var, &chan, 0, 7, 0)) {
		  fprintf(stderr, TOK_ERR_ULONG_MSG_FMT, "Environment variable \'UMD_CHAN\'", 0, 7);
		  exit(EXIT_FAILURE);
	  }
  }

  RIOMemOpsIntf* mops = RIOMemOps_classFactory(met[m], 0, chan);

  printf("HW access method=%s %s destid=%u rio_addr=0x%llx [chan=%d]\n", met_str[m], sync_str, did, rio_addr, chan);

  if (met[m] == MEMOPS_UMD) {
    RIOMemOpsUMD* mops_umd = dynamic_cast<RIOMemOpsUMD*>(mops);
    bool r = mops_umd->setup_channel(0x100 /*bufc*/, 0x400 /*sts aka FIFO*/);
    assert(r);
    r = mops_umd->start_fifo_thr(-1 /*isolcpu*/);
    assert(r);
  }

  DmaMem_t ibmem; memset(&ibmem, 0, sizeof(ibmem));
  ibmem.rio_address = RIO_ANY_ADDR;
  mops->alloc_dmawin(ibmem, 40960);
  printf("IBWin RIO addr @0x%lx size 0x%x\n", ibmem.win_handle, ibmem.win_size);

  const int TR_SZ = 256;

  MEMOPSRequest_t req; memset(&req, 0, sizeof(req));
  req.destid = did;
  req.bcount = TR_SZ;
  req.raddr.lsb64 = rio_addr;
  req.mem.rio_address = RIO_ANY_ADDR;
  mops->alloc_dmawin(req.mem, 40960);
  req.mem.offset = TR_SZ;
  req.sync       = sync;
  req.wr_mode    = RIO_DIRECTIO_TYPE_NWRITE_R;

// NWRITE_R
  uint8_t* p = (uint8_t*)req.mem.win_ptr;
  p[TR_SZ] = 0xdb;
  p[TR_SZ+1] = 0xae;

  if (! mops->nwrite_mem(req)) {
    int abort = mops->getAbortReason();
    fprintf(stderr, "NWRITE_R failed with reason %d (%s)\n", abort, mops->abortReasonToStr(abort));
    ret = 1;
    goto done;
  }
 
  if (mops->canRestart() && mops->checkAbort()) {
      int abort = mops->getAbortReason();
      fprintf(stderr, "NWRITE_R ABORTed with reason %d (%s)\n", abort, mops->abortReasonToStr(abort));
      mops->restartChannel();
      ret = 41;
      goto done;
  }

  if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
    bool r = mops->wait_async(req, timeout);
    if (!r) {
      int abort = mops->getAbortReason();
      fprintf(stderr, "NWRITE_R async wait failed after %dms with reason %d (%s)\n", timeout, abort, mops->abortReasonToStr(abort));
      ret = 42;
      goto done;
    }
  }

// NREAD
  req.raddr.lsb64 = rio_addr + 256; 
  req.mem.offset = 0;

  if (! mops->nread_mem(req)) {
    int abort = mops->getAbortReason();
    fprintf(stderr, "NREAD failed with reason %d (%s)\n", abort, mops->abortReasonToStr(abort));
    ret = 1;
    goto done;
  } 

  if (mops->canRestart() && mops->checkAbort()) {
      int abort = mops->getAbortReason();
      fprintf(stderr, "NREAD ABORTed with reason %d (%s)\n", abort, mops->abortReasonToStr(abort));
      mops->restartChannel();
      ret = 41;
      goto done;
  }

  if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
    bool r = mops->wait_async(req, timeout);
    if (!r) {
      int abort = mops->getAbortReason();
      fprintf(stderr, "NREAD async wait failed after %dms with reason %d (%s)\n", timeout, abort, mops->abortReasonToStr(abort));
      ret = 42;
      goto done;
    }
  }

  printf("Mem-in:\n");
  for (int i = 0; i < TR_SZ; i++) {
    printf("%02x ", p[i]);
    if (0 == ((i+1) % 32))
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
