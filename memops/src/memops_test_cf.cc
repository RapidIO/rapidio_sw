#include "memops.h"

int main(int argc, char* argv[])
{
  const int mport_num = 0;
  RIOMemOpsIntf* memops = NULL;

  memops = RIOMemOpsChanMgr(mport_num, false /*shared*/, 1);
  delete memops; memops = NULL;

  return 0;
}
