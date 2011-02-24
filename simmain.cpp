#ifdef SIMULATE

#include "hwdep.h"
#include "timing.h"

volatile static int32 i;

void second_int() {
  debug_long(time_get_ns()); debug("\n");
}

void gps_int() {
  uint32 ns = time_get_ns();
  debug("Got GPS int, ns="); debug_long(ns); debug("\n");
}

int main() {
  timer_init();

  for (short j = -100 ; j < 100 ; j++) {
    tickadj_set_ppm(j);
    sec = 0;
    uint32 startns = time_get_ns();

    for (int k = 0 ; k <= F_CPU ; k++) {
      timer_clk();
    }

    uint32 secdiff = sec;
    int32 nsdiff = time_get_ns() - startns;
    if (nsdiff < 0) {
      secdiff ++;
      nsdiff -= 1000000000L;
    }
    printf("%5d: %ld.%09ld\n", j, secdiff, nsdiff);
  }
}

#endif

