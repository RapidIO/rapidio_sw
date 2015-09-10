/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
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
#include "dmadesc.h"

void hexdump4byte(uint8_t* d, int len)
{
  printf("Mem @%p size %d:\n", d, len);
  for(int i = 0; i < len; i++) {
    printf("%02x ", d[i]);
    if(((i + 1) % 4) == 0) printf("\n");
  }
}

int main(int argc, char* argv[])
{
  if(argc < 5) {
    fprintf(stderr, "usage: %s devid(d) raddr(h) buffp(h) bcount(d/h)\n", argv[0]);
    return 0;
  }

  int n = 1;

  uint8_t rtype = 2; // ALL_NWRITE
  uint32_t devid = 0;
  uint64_t raddr = 0;
  uint64_t buffp = 0;
  uint32_t bcount = 0;

  { sscanf(argv[n++], "%d", &devid); printf("devid = 0x%x\n", devid); }
  { sscanf(argv[n++], "%lx", &raddr); printf("raddr = 0x%lx\n", raddr); }
  { sscanf(argv[n++], "%lx", &buffp); printf("bufptr = 0x%lx\n", buffp);}
  {
    const char* arg = argv[n++];
    char* fmt = "%d";
    if(!strncmp(arg, "0x", 2) || !strncmp(arg, "0X", 2)) fmt = "%x";
    sscanf(arg, fmt, &bcount);
    printf("bcount = 0x%x\n", bcount);
  }

  dmadesc t1;

  dmadesc_setdtype(t1, 1);
  dmadesc_setrtype(t1, rtype); // NWRITE_ALL

  dmadesc_set_raddr(t1, 0, raddr);

  dmadesc_setdevid(t1, devid);

  dmadesc_setT1_bufptr(t1, buffp);
  dmadesc_setT1_buflen(t1, bcount);

  {
  struct hw_dma_desc desc;
  t1.pack(&desc);
  printf("Vlad T1:"); hexdump4byte((uint8_t*)&desc, sizeof(desc));
  }

  dmadesc t3;
  dmadesc_setdtype(t3, 3);
  dmadesc_setT3_nextptr(t3, buffp); // XXX mask off lowest 5 bits

  {
  struct hw_dma_desc desc;
  t3.pack(&desc);
  printf("Vlad T3:"); hexdump4byte((uint8_t*)&desc, sizeof(desc));
  }

  return 0;
}
