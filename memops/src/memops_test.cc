#include <stdio.h>

#include "memops.h"

void usage(const char* name)
{
  fprintf(stderr, "usage: %s <method> <destid> <hexrioaddr>\n", name);
  exit(0);
}

MEMOPSAccess_t met[] = { MEMOPS_MPORT, MEMOPS_UMDD, MEMOPS_UMD };

int main(int argc, char* argv[])
{
  if (argc < 4) usage(argv[0]);

  int ret = 0;
  int n = 1;

  int m = atoi(argv[n++]);
  if (m < 0 || m > 2) return 1;
  
  uint16_t did = atoi(argv[n++]);

  uint64_t rio_addr = 0;
  sscanf(argv[n++], "%llx", &rio_addr);

  printf("Method=%d destid=%u rio_addr=0x%llx\n", m, did, rio_addr);

  RIOMemOpsIntf* mops = RIOMemOps_classFactory(met[m], 0, 7);

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
  req.sync       = RIO_DIRECTIO_TRANSFER_SYNC;
  req.wr_mode    = RIO_DIRECTIO_TYPE_NWRITE_R;

  uint8_t* p = (uint8_t*)req.mem.win_ptr;
  p[TR_SZ] = 0xdb;
  p[TR_SZ+1] = 0xae;

  if (! mops->nwrite_mem(req)) {
    int abort = mops->getAbortReason();
    fprintf(stderr, "NWRITE_R failed with reason %d (%s)\n", abort, mops->abortReasonToStr(abort));
    ret = 1;
    goto done;
  }
 
  req.raddr.lsb64 = rio_addr + 256; 
  req.mem.offset = 0;

  if (! mops->nread_mem(req)) {
    int abort = mops->getAbortReason();
    fprintf(stderr, "NWRITE_R failed with reason %d (%s)\n", abort, mops->abortReasonToStr(abort));
    ret = 1;
    goto done;
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
