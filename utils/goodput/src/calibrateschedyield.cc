#include <stdio.h>
#include <stdint.h>
#include <sched.h>

#include "rdtsc.h"

int main()
{
  uint64_t dRDTSC = 0;
  for(int i = 0; i < 10000; i++) {
    do {
      register uint64_t t1 = rdtsc();
      register uint64_t t2 = rdtsc();
      if(t2 < t1 && (t2-t1) <= 10000) continue;

      dRDTSC += t2-t1;
      break;
    } while (1);
  }
  dRDTSC /= 10000;

  uint64_t count = 0;
  uint64_t min=0, max = 0, dSY = 0;

  for(int i = 0; i < 10000000; i++) {
    register uint64_t t1,t2;
    do {
      t1 = rdtsc();
      sched_yield();
      t2 = rdtsc();
      if(t2 > t1) break;
    } while (1);     
    int64_t dT = t2 - t1 - dRDTSC;
    if(dT < 0) continue;

    if(count == 0) min = dT;
    dSY += dT;
    count++;
    if(dT < min) { min = dT; };
    if(dT > max) { max = dT; };
  }

  double avg = (dSY * 1.0) / count;

  printf("sched_yield ticks min=%llu max=%llu avg=%f\n", min, max, avg);
  return 0;
}
