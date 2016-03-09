#include "dmachan.h"

//unsigned g_level = 7;

int main(int argc, char* argv[])
{
  void* dch = DMAChannel_create(0, 7);

  DMAChannel_pingMaster(dch);

  DMAChannel_destroy(dch);

  return 0;
}
