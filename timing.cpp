#include "hwdep.h"

#include <string.h>

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

inline int32 med_mean_filter(int32 history[5]) {
  int32 tmp;
  int32 copy[5];
  memcpy(copy, history, 5 * sizeof(int32));

  for (char i = 0 ; i < 4 ; i++) {
    for (char j = i + 1 ; j < 5 ; j++) {
      if (copy[j] < copy[i]) {
        tmp = copy[i];
        copy[i] = copy[j];
        copy[j] = tmp;
      }
    }
  }

  return ((copy[1] + copy[2] + copy[3]) / 3);
}

volatile extern char pps_int;
volatile extern uint32 pps_ns;
volatile extern char ints;

static int32 pps_ns_copy = 0;
static int32 pps_history[5];
static short last_slew_rate = 0;
static int32 ppschange_int;

static short clocks = -3439; /* 213.2 ppm */
//static short clocks = 0;

// One half of a timer interrupt to minimize the odds
// of having a PPS int hit within a few cycles of a timer int
#define PLL_OFFSET 15625000
#define PLL_RATE_DIV 3072L

void pll_run() {
  pps_int = 0;
  pps_ns_copy = pps_ns - PLL_OFFSET;
  if (pps_ns_copy < 1000000000L)
    pps_ns_copy += 1000000000L;

  if (pps_ns_copy > 1000000000L)
    pps_ns_copy -= 1000000000L;

  if (pps_ns_copy > 500000000L) {
    pps_ns_copy -= 1000000000L;
  }

  if (pps_ns_copy == -31250000L) {
    pps_ns_copy = 0;
  }

  debug("PPS: "); debug_long(pps_ns_copy); debug("\n");

  pps_history[4] = pps_history[3];
  pps_history[3] = pps_history[2];
  pps_history[2] = pps_history[1];
  pps_history[1] = pps_history[0];
  pps_history[0] = pps_ns_copy;

  int32 pps_filtered = med_mean_filter(pps_history);
  debug("PPS filtered: "); debug_long(pps_filtered); debug("\n");

  short slew_rate = 0;
  char hardslew = 0;

  if (pps_ns_copy < -32500000L && pps_filtered < -32500000L) {
    ints++;
    hardslew = 1;
    ppschange_int = 0;
    debug("Slew ------\n");
  } else if (pps_ns_copy > 32500000L && pps_filtered > 32500000L) {
    ints--;
    hardslew = 1;
    ppschange_int = 0;
    debug("Slew ++++++\n");
  } else if (pps_filtered > 2048 || pps_filtered < -2048) {
    slew_rate = pps_filtered / 2048;
    if (pps_filtered > 10000) {
      slew_rate += 50;
    } else if (pps_filtered < -100000) {
      slew_rate -= 50;
    }
    if (slew_rate > 4000)
      slew_rate = 4000;
    if (slew_rate < -4000)
      slew_rate = -4000;
    debug("Slew "); debug_int(slew_rate); debug("\n");
  } else {
    debug("Slew 0\n");
  }

/* The ideal factor for last_slew_rate here would be 62.5 (1000 / 16) but we
 * undercorrect a little bit, leaving a residual that will help to keep us from
 * settling in a state where slew != 0
 */
  int32 ppschange = pps_ns_copy - pps_history[1] + (int32)last_slew_rate * 61;
//  debug("PPS change raw: "); debug_long(ppschange); debug("\n");
  ppschange_int += ppschange;
  debug("PPS change integrated: "); debug_long(ppschange_int); debug("\n");

  if (!hardslew && ppschange_int < -PLL_RATE_DIV) {
    if (ppschange_int < -16 * PLL_RATE_DIV) {
      debug("Speed ++\n");
      clocks -= 16;
      ppschange_int = 0;
    } else {
      debug("Speed +\n");
      clocks += ppschange_int / PLL_RATE_DIV;
      ppschange_int -= PLL_RATE_DIV * (ppschange_int / PLL_RATE_DIV);
    }
  } else if (!hardslew && ppschange_int > PLL_RATE_DIV) {
    if (ppschange_int > 16 * PLL_RATE_DIV) {
      debug("Speed --\n");
      clocks += 16;
      ppschange_int = 0;
    } else {
      debug("Speed -\n");
      clocks += ppschange_int / PLL_RATE_DIV;
      ppschange_int -= PLL_RATE_DIV * (ppschange_int / PLL_RATE_DIV);
    }
  } else {
    debug("Speed =\n");
  }

  last_slew_rate = slew_rate;

  debug("PLL: "); debug_int(clocks); debug("\n");
  tickadj_set_clocks(clocks + slew_rate);
}
