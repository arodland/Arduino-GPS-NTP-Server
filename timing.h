#ifndef __TIMING_H
#define __TIMING_H

#include "hwdep.h"

extern uint32 make_ns(unsigned char ints, unsigned short counter);
extern uint32 time_get_ns();
extern void tickadj_set_ppm(signed short ppm);
extern void tickadj_set_clocks(signed short clocks);

#endif
