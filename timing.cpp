#include "hwdep.h"

volatile /* static */ char ints = 0;
/* static */ char sec = 0;
static signed char tickadj_upper = 0;
static unsigned char tickadj_lower = 0;
static unsigned char tickadj_phase = 0;

int32 make_ns(unsigned char ints, unsigned short counter) {
  return ints * NS_PER_INT + counter * NSPC(timer_get_interval());
}

int32 time_get_ns() {
  return make_ns(ints + timer_get_pending(), timer_get_counter());
}

int32 time_get_ns_capt() {
  char i = ints;
  unsigned short ctr = timer_get_capture();
  if (ctr > timer_get_counter()) { /* Timer reset after capture */
    i--;
  }
  return make_ns(i, timer_get_capture());
}

void tickadj_adjust() {
  int x = (tickadj_phase / 16 < tickadj_lower % 16) ? 1 : 0;
  int y = (tickadj_phase % 16 < (tickadj_lower / 16) + x);

  /* these actually give periods of DEF_TIMER_VAL + tickadj_upper and
   * DEF_TIMER_VAL + tickadj_upper + 1 because the timer adds one. This means
   * the avg. period is (DEF_TIMER_VAL + tickadj_upper + tickadj_lower / 256)
   * and the frequency is 2MHz / that.
   */
  if (y) {
    timer_set_interval(DEF_TIMER_VAL + tickadj_upper + 1);
  } else {
    timer_set_interval(DEF_TIMER_VAL + tickadj_upper);
  }
}

void tickadj_run() {
  tickadj_phase++;
//  debug("PLL phase "); debug_int(tickadj_phase); debug("\n");
  tickadj_adjust();
}

extern void second_int();

static int ledstate = 0;

void timer_int() {
  ints++;
  if (ints == INT_PER_SEC) {
    sec++;
    ints = 0;
    second_int();
  }

#ifndef SIMULATE
  ledstate++;
  digitalWrite(13, (ledstate & 4) ? 1 : 0);
#endif
  tickadj_run();
}

void tickadj_set(signed char upper, unsigned char lower) {
  tickadj_upper = upper;
  tickadj_lower = lower;
//  debug("tickadj_upper = "); debug_int(tickadj_upper);
//  debug(", tickadj_lower = "); debug_int((short)tickadj_lower); debug("\n");
  tickadj_adjust();
}

void tickadj_set_clocks(signed short clocks) {
  char negative = 0;
  debug("tickadj = "); debug_int(clocks); debug("\n");
  if (clocks < 0) {
    negative = 1;
    clocks = 0 - clocks;
  }
  
  signed char upper = clocks / 256;
  unsigned char lower = clocks % 256;
  
  if (negative) {
    upper = (0 - upper) - 1;
    lower = 0 - lower;
    /* lower is always interpreted as addition, so the opposite of
       e.g. 1 + 16/256 is -2 + 240/256.
    */
  }
  tickadj_set(upper, lower);
}

/* Positive PPM will make the clock run fast, negative slow */
void tickadj_set_ppm(signed short ppm) {
  /* Negation is a shortcut for computing 1 / ((1M + ppm) / 1M) that's accurate
   * out to a few hundred, which is all we want. From her on out the clock speed
   * will actually be *divided* by (1000000 + ppm) / 10000000
   */

  if (ppm < -2047 || ppm > 2047) {
    debug("time adjustment out of range!\n");
  } else {
    signed short clocks = -16 * ppm;
    tickadj_set_clocks(clocks);
  }
}

volatile extern char pps_int;
volatile extern uint32 pps_ns;
volatile extern char ints;

static int32 last_pps_ns = 0;
static int32 pps_ns_copy = 0;
static short last_slew_rate = 0;
static int32 ppschange_int;

//unsigned short clocks = -3439; /* 213.2 ppm */
static short clocks = 0;

// One half of a timer interrupt to minimize the odds
// of having a PPS int hit within a few cycles of a timer int
#define PLL_OFFSET 15625000

void pll_run() {
  pps_int = 0;
  pps_ns_copy = pps_ns - PLL_OFFSET;
  if (pps_ns_copy < 1000000000L)
    pps_ns_copy += 1000000000L;

  if (pps_ns_copy > 500000000L) {
    pps_ns_copy -= 1000000000L;
  }

  if (pps_ns_copy == -31250000L) {
    pps_ns_copy = 0;
  }

  debug("PPS: "); debug_long(pps_ns_copy); debug("\n");

  short slew_rate = 0;
  char hardslew = 0;

  if (pps_ns_copy < -32500000L) {
    ints++;
    hardslew = 1;
    debug("Hard slew -\n");
  } else if (pps_ns_copy > 32500000L) {
    ints--;
    hardslew = 1;
    debug("Hard slew +\n");
  } else if (pps_ns_copy > 2048 || pps_ns_copy < -2048) {
    slew_rate = pps_ns_copy / 2048;
    if (pps_ns_copy > 10000) {
      slew_rate += 100;
    } else if (pps_ns_copy < -100000) {
      slew_rate -= 100;
    }
    if (slew_rate > 2000)
      slew_rate = 2000;
    if (slew_rate < -2000)
      slew_rate = -2000;
    debug("Slew "); debug_int(slew_rate); debug("\n");
  }

  int32 ppschange = pps_ns_copy - last_pps_ns + (int32)last_slew_rate * 1000 / 16;
  debug("PPS change raw: "); debug_long(ppschange); debug("\n");
  ppschange_int += ppschange;
  debug("PPS change integrated: "); debug_long(ppschange_int); debug("\n");

  if (!hardslew && ppschange_int < -4096) {
    if (ppschange_int < -40960) {
      debug("Speed up max\n");
      clocks -= 10;
      ppschange_int = 0;
    } else {
      debug("Speed up\n");
      clocks += ppschange_int / 4096;
      ppschange_int -= 4096 * (ppschange_int / 4096);
    }
  } else if (!hardslew && ppschange_int > 4096) {
    if (ppschange_int > 40960) {
      debug("Slow down max\n");
      clocks += 10;
      ppschange_int = 0;
    } else {
      debug("Slow down\n");
      clocks += ppschange_int / 4096;
      ppschange_int -= 4096 * (ppschange_int / 4096);
    }
  }

  last_slew_rate = slew_rate;
  last_pps_ns = pps_ns_copy;

  debug("PLL: "); debug_int(clocks); debug("\n");
  tickadj_set_clocks(clocks + slew_rate);
}
