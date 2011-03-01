#ifndef __TIMING_H
#define __TIMING_H

#include "hwdep.h"

extern int32 make_ns(unsigned char ints, unsigned short counter);
extern int32 time_get_ns();
extern int32 time_get_ns_capt();
extern void tickadj_set_ppm(signed short ppm);
extern void tickadj_set_clocks(signed short clocks);
extern void pll_run();

#endif
