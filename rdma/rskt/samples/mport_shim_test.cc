#include <stdio.h>
#include "mport_shim.h"

int main()
{
  printf("my destid %u\n", mport_my_destid());
  return 0;
}
