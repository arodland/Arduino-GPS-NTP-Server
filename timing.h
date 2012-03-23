#ifndef __TIMING_H
#define __TIMING_H

#include "hwdep.h"

extern int32 make_ns(unsigned char ints, unsigned short counter);
extern int32 time_get_ns();
extern int32 time_get_ns_capt();
extern void tickadj_set_ppm(signed short ppm);
extern void tickadj_set_clocks(signed short clocks);
extern void pll_run();

extern void time_set_date(unsigned int gps_week, uint32 gps_tow_sec, int offset);

void time_get_ntp(uint32 *upper, uint32 *lower, int32 fudge);
uint32 time_get_ntp_lower(int32 fudge);

#endif
