#include "rio_misc.h"
#include "memops.h"

int main(int UNUSED(argc), char **UNUSED(argv))
{
  const int mport_num = 0;
  RIOMemOpsIntf* memops = NULL;

  memops = RIOMemOpsChanMgr(mport_num, false /*shared*/, 1);
  delete memops; memops = NULL;

  return 0;
}
