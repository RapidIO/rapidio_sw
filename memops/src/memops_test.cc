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

#include "memops.h"
#include "memops_umd.h"

int timeout = 1000; // miliseconds

void usage(const char* name)
{
  fprintf(stderr, "usage: %s [-A|-a|-F] <method> <destid> <hexrioaddr>\n" \
                  "\t\t-A async transaction, blocking forever\n" \
                  "\t\t-a async transaction, blocking %dms\n" \
                  "\t\t-F faf transaction\n" \
                  "\t\tNote: default is sync\n" \
                  "\t\tMethod: 0=mport, 1=UMDd/SHM, 2=UMD\n", name, timeout);
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
  */

  const char* sync_str = "SYNC";
  enum riomp_dma_directio_transfer_sync sync = RIO_DIRECTIO_TRANSFER_SYNC;

  int ret = 0;
  int n = 1;

  if (argv[n][0] == '-') {
    if (argc < 5 || strlen(argv[n]) < 2) usage(argv[0]);

    switch(argv[n][1]) {
      case 'A': sync = RIO_DIRECTIO_TRANSFER_ASYNC; timeout = 0; sync_str = "ASYNC(inft)"; break;
      case 'a': sync = RIO_DIRECTIO_TRANSFER_ASYNC; sync_str = "ASYNC(tmout)"; break;
      case 'F': sync = RIO_DIRECTIO_TRANSFER_FAF; sync_str = "FAF"; break;
      default: fprintf(stderr, "%s: Invalid option %s\n", argv[0], argv[n]);
               usage(argv[0]);
               break;
    }
    n++;
  }
  int m = atoi(argv[n++]);
  if (m < 0 || m > 2) return 1;
  
  uint16_t did = atoi(argv[n++]);

  uint64_t rio_addr = 0;
  sscanf(argv[n++], "%llx", &rio_addr);

  int chan = 7;
  if (getenv("UMD_CHAN") != NULL) chan = atoi(getenv("UMD_CHAN"));

  printf("HW access method=%s %s destid=%u rio_addr=0x%llx [chan=%d]\n", met_str[m], sync_str, did, rio_addr, chan);

  RIOMemOpsIntf* mops = RIOMemOps_classFactory(met[m], 0, chan);

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
      ret = 41; goto done;
  }

  if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
    bool r = mops->wait_async(req, timeout);
    if (!r) {
      int abort = mops->getAbortReason();
      fprintf(stderr, "NWRITE_R async wait failed after %dms with reason %d (%s)\n", timeout, abort, mops->abortReasonToStr(abort));
      ret = 42; goto done;
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
      ret = 41; goto done;
  }

  if (sync == RIO_DIRECTIO_TRANSFER_ASYNC) {
    bool r = mops->wait_async(req, timeout);
    if (!r) {
      int abort = mops->getAbortReason();
      fprintf(stderr, "NREAD async wait failed after %dms with reason %d (%s)\n", timeout, abort, mops->abortReasonToStr(abort));
      ret = 42; goto done;
    }
  }

  printf("Mem-in:\n");
  for (int i = 0; i < TR_SZ; i++) {
    printf("%02x ", p[i]);
    if (0 == ((i+1) % 32)) printf("\n");
  }
  printf("\n");

done:
  delete mops;

  return ret;
}
