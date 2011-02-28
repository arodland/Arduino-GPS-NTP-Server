#ifdef SIMULATE

#include <unistd.h>
#include "hwdep.h"
#include "timing.h"

volatile extern char pps_int;

void second_int() {
//  usleep(2500);
}

int main() {
  timer_init();
  tickadj_set_clocks(0);

  for(;;) {
    sim_clk();
    if (pps_int) {
      pll_run();
    }
  }
}

#endif

