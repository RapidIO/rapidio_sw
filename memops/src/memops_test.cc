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

  int n = 1;

  int m = atoi(argv[n++]);
  if (m < 0 || m > 2) return 1;
  
  uint16_t did = atoi(argv[n++]);

  uint64_t rio_addr = 0;
  sscanf(argv[n++], "%llx", &rio_addr);


  printf("Method=%d destid=%u rio_addr=0x%llx\n", m, did, rio_addr);


  DmaMem_t dmem; memset(&dmem, 0, sizeof(dmem));
  dmem.rio_address = RIO_ANY_ADDR;

  RIOMemOpsIntf* mops = RIOMemOps_classFactory(met[m], 0, 7);
  mops->alloc_dmawin(dmem, 40960);
  delete mops;

  return 0;
}
