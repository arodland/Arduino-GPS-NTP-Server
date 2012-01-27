#ifdef SIMULATE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "hwdep.h"
#include "timing.h"
#include "gps.h"

volatile extern char pps_int;

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  srand(42);
  timer_init();
  gps_init();
  tickadj_set_clocks(0);
  int cycles = 0;

  for(;;) {
    sim_clk();
    if (pps_int) {
      cycles ++;
      printf("Cycle: %d\n", cycles);
      pll_run();
    }
    gps_poll();
  }
}

#endif

