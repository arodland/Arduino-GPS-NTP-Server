#include "hwdep.h"
#include "tempprobe.h"

#include <string.h>

volatile /* static */ char ints = 0;
/* For timing medium-resolution events like tempprobe reading and DHCP
 * renewal that shouldn't run with interrupts disabled
 */
volatile char schedule_ints = 0;
static signed char tickadj_upper = 0;
static unsigned char tickadj_lower = 0;
static unsigned char tickadj_phase = 0;

static unsigned int gps_week = 0;
static uint32 tow_sec_utc = 0;

void time_set_date(unsigned int week, uint32 gps_tow, int offset) {
  if ((int32)gps_tow + offset < 0) {
    tow_sec_utc = gps_tow + 604800L + offset;
    gps_week = week - 1;
  } else {
    tow_sec_utc = gps_tow + offset;
    gps_week = week;
  }
}

const uint32 PLL_OFFSET_NS = 15625000L;
const uint32 PLL_OFFSET_NTP = 0x4000000UL;

int32 make_ns(unsigned char i, unsigned short counter) {
  int32 ns = i * NS_PER_INT + counter * NSPC(timer_get_interval());
  if (ns + PLL_OFFSET_NS > 1000000000L) {
    ns -= 1000000000L;
  }
  ns += PLL_OFFSET_NS;
  return ns;
}

int32 make_ns_carry(unsigned char i, unsigned short counter, char *add_sec) {
  int32 ns = i * NS_PER_INT + counter * NSPC(timer_get_interval());
  if (ns + PLL_OFFSET_NS > 1000000000L) {
    ns -= 1000000000L;
    *add_sec = 1;
  } else
    *add_sec = 0;
  ns += PLL_OFFSET_NS;
  return ns;
}

uint32 make_ntp(unsigned char i, unsigned short counter) {
  uint32 ntp = i * NTP_PER_INT + counter * NTPPC(timer_get_interval());
  ntp += PLL_OFFSET_NTP;
  return ntp;
}

uint32 make_ntp_carry(unsigned char i, unsigned short counter, char *add_sec) {
  uint32 ntp = i * NTP_PER_INT + counter * NTPPC(timer_get_interval());
  uint32 ntp_augmented = ntp + PLL_OFFSET_NTP;
  *add_sec = ntp_augmented < ntp;
  return ntp_augmented;
}

int32 time_get_ns() {
  char i = ints + timer_get_pending();
  unsigned short ctr = timer_get_counter();
  return make_ns(i, ctr);
}

uint32 time_get_ntp_lower() {
  char i = ints + timer_get_pending();
  unsigned short ctr = timer_get_counter();
  return make_ntp(i, ctr);
}

void time_get_ntp(uint32 *upper, uint32 *lower) {
  char add_sec;
  *upper = 2524953600UL; /* GPS epoch - NTP epoch */
  *upper += gps_week * 604800UL; /* 1 week */
  *upper += tow_sec_utc;

  char i = ints + timer_get_pending();
  unsigned short ctr = timer_get_counter();
  *lower = make_ntp_carry(i, ctr, &add_sec);
  *upper += add_sec;
}

int32 time_get_ns_capt() {
  char i = ints;
  unsigned short ctr = timer_get_capture();
  if (ctr > timer_get_counter()) { /* Timer reset after capture */
    i--;
  }
  return make_ns(i, ctr);
}

uint32 ns_to_ntp(uint32 ns) {
  const unsigned int chi = 0x4b82;
  const unsigned int clo = 0xfa09;
  const unsigned int xhi = ns >> 16;
  const unsigned int xlo = ns & 0xffff;
  uint32 crossTerms = xhi*clo + xlo*chi;
  uint32 highTerm = xhi*chi;
  return (ns << 2) + highTerm + (crossTerms >> 16) + 1;
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

void second_int() {
  ++tow_sec_utc;
  if (tow_sec_utc >= 604800L) {
    tow_sec_utc -= 604800L;
    ++gps_week;
  }
}

static int ledstate = 0;

void timer_int() {
  ints++;
  if (ints == INT_PER_SEC) {
    ints = 0;
    second_int();
  }
  tickadj_run();

#ifndef SIMULATE
  ledstate++;
  digitalWrite(13, (ledstate & 4) ? 1 : 0);
#endif
  schedule_ints++;
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
//  debug("tickadj = "); debug_int(clocks); debug("\n");
  union {
    struct {
      unsigned char lower;
      signed char upper;
    };
    unsigned short whole;
  } pun;
  pun.whole = clocks;
  tickadj_set(pun.upper, pun.lower);
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

inline int32 median_filter(int32 history[5]) {
  int32 low = history[0], mid = history[1], high = history[2];
  if (low > mid) {
    int32 tmp = low;
    low = mid;
    mid = tmp;
  }
  if (mid < high) {
    return mid;
  } else {
    if (high > low) {
      return high;
    } else {
      return low;
    }
  }
}



volatile extern char pps_int;
volatile extern uint32 pps_ns;
volatile extern char ints;

static int32 pps_ns_copy = 0;
static int32 pps_history[5];
static short last_slew_rate = 0;
static int32 ppschange_int;
static char lasthardslew = 0;

static short clocks = -3439; /* 213.2 ppm */
//static short clocks = 0;

#define PLL_SLEW_DIV 500L
#define PLL_SLEW_THRESH 1000L
#define PLL_SLEW_MAX 8192L
#define PLL_RATE_DIV 2048L

void pll_run() {
  pps_int = 0;
  pps_ns_copy = pps_ns;
  if (pps_ns_copy < 1000000000L)
    pps_ns_copy += 1000000000L;

  if (pps_ns_copy > 1000000000L)
    pps_ns_copy -= 1000000000L;

  if (pps_ns_copy > 500000000L) {
    pps_ns_copy -= 1000000000L;
  }

  debug("PPS: "); debug_long(pps_ns_copy); debug("\n");

  pps_history[4] = pps_history[3];
  pps_history[3] = pps_history[2];
  pps_history[2] = pps_history[1];
  pps_history[1] = pps_history[0];
  pps_history[0] = pps_ns_copy;

//  int32 pps_filtered = med_mean_filter(pps_history);
  int32 pps_filtered = median_filter(pps_history);
//  debug("PPS filtered: "); debug_long(pps_filtered); debug("\n");

  short slew_rate = 0;
  char hardslew = 0;

  if (pps_ns_copy < -32500000L && pps_filtered < -32500000L) {
    ints++;
    hardslew = -1;
    ppschange_int = 0;
    clocks = 0;
//    debug("Slew ------\n");
  } else if (pps_ns_copy > 32500000L && pps_filtered > 32500000L) {
    ints--;
    hardslew = 1;
    ppschange_int = 0;
    clocks = 0;
//    debug("Slew ++++++\n");
  } else if (pps_filtered >= PLL_SLEW_THRESH || pps_filtered <= -PLL_SLEW_THRESH) {
    if (pps_filtered >= PLL_SLEW_MAX * PLL_SLEW_DIV) {
      slew_rate = PLL_SLEW_MAX;
    } else if (pps_filtered <= -PLL_SLEW_MAX * PLL_SLEW_DIV) {
      slew_rate = -PLL_SLEW_MAX;
    } else {
      slew_rate = pps_filtered / PLL_SLEW_DIV + ((pps_filtered > 0) ? 2 : -2);
      if (pps_filtered > 10000) {
        slew_rate += 50;
      } else if (pps_filtered < -100000) {
        slew_rate -= 50;
      }
    }
//    debug("Slew "); debug_int(slew_rate); debug("\n");
  } else if (pps_filtered > 0) {
    slew_rate = 1;
  } else if (pps_filtered < 0) {
    slew_rate = -1;
  }

  if (slew_rate >= -6 && slew_rate <= 6) {
    ppschange_int += pps_ns_copy / 50 + pps_filtered / 25;
  }

  if (!hardslew && lasthardslew) {
    int32 ppschange = pps_ns_copy - pps_history[1] + 
      (int32)lasthardslew * 31250000L;
    /* 62.5 ns per clock */
    clocks = (ppschange * 2) / 125;
  } else if (!hardslew) {
    /* The ideal factor for last_slew_rate here would be 62.5 (1000 / 16) but we
     * undercorrect a little bit, leaving a residual that will help to keep us
     * from settling in a state where slew != 0
     */
    int32 ppschange = pps_ns_copy - pps_history[1] + (int32)last_slew_rate * 61;
    // debug("PPS change raw: "); debug_long(ppschange); debug("\n");
    ppschange_int += ppschange;
    // debug("PPS change integrated: "); debug_long(ppschange_int); debug("\n");
    if (ppschange_int < -PLL_RATE_DIV) {
      if (ppschange_int < -16 * PLL_RATE_DIV) {
        // debug("Speed ++\n");
        clocks -= 16;
        ppschange_int = 0;
      } else {
        // debug("Speed +\n");
        clocks += ppschange_int / PLL_RATE_DIV;
        ppschange_int -= PLL_RATE_DIV * (ppschange_int / PLL_RATE_DIV);
      }
    } else if (ppschange_int > PLL_RATE_DIV) {
      if (ppschange_int > 16 * PLL_RATE_DIV) {
        // debug("Speed --\n");
        clocks += 16;
        ppschange_int = 0;
      } else {
        // debug("Speed -\n");
        clocks += ppschange_int / PLL_RATE_DIV;
        ppschange_int -= PLL_RATE_DIV * (ppschange_int / PLL_RATE_DIV);
      }
    } else {
      // debug("Speed =\n");
    }
  }

  last_slew_rate = slew_rate;
  lasthardslew = hardslew;

  debug("PLL: "); debug_int(clocks); debug("\n");
  debug("Temp: "); debug_float(tempprobe_gettemp()); debug("\n");

  tickadj_set_clocks(clocks + slew_rate);
}
