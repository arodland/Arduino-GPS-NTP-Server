#include "hwdep.h"
#include "tempprobe.h"

#include <string.h>

#define FRACTIONAL_COMP 1

volatile /* static */ char ints = 0;
/* For timing medium-resolution events like tempprobe reading and DHCP
 * renewal that shouldn't run with interrupts disabled
 */
volatile char schedule_ints = 0;
static signed char tickadj_upper = 0;
static unsigned char tickadj_lower = 0;
static unsigned char tickadj_accum = 0;
static unsigned char tickadj_extra = 0;

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
  unsigned short tm = timer_get_interval();
  int32 ns = i * NS_PER_INT + counter * NSPC(tm);
#ifdef FRACTIONAL_COMP
  ns -= NSPADJ(tm) * tickadj_accum;
#endif
  if (ns + PLL_OFFSET_NS > 1000000000L) {
    ns -= 1000000000L;
  }
  ns += PLL_OFFSET_NS;
  return ns;
}

int32 make_ns_carry(unsigned char i, unsigned short counter, char *add_sec) {
  unsigned short tm = timer_get_interval();
  int32 ns = i * NS_PER_INT + counter * NSPC(tm);
#ifdef FRACTIONAL_COMP
  ns -= NSPADJ(tm) * tickadj_accum;
#endif
  if (ns + PLL_OFFSET_NS > 1000000000L) {
    ns -= 1000000000L;
    *add_sec = 1;
  } else
    *add_sec = 0;
  ns += PLL_OFFSET_NS;
  return ns;
}

uint32 make_ntp(unsigned char i, unsigned short counter) {
  unsigned short tm = timer_get_interval();
  uint32 ntp = i * NTP_PER_INT + counter * NTPPC(tm);
#ifdef FRACTIONAL_COMP
  ntp -= NTPPADJ(tm) * tickadj_accum;
#endif
  ntp += PLL_OFFSET_NTP;
  return ntp;
}

uint32 make_ntp_carry(unsigned char i, unsigned short counter, char *add_sec) {
  unsigned short tm = timer_get_interval();
  uint32 ntp = i * NTP_PER_INT + counter * NTPPC(tm);
#ifdef FRACTIONAL_COMP
  ntp -= NTPPADJ(tm) * tickadj_accum;
#endif
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
  timer_set_interval(DEF_TIMER_VAL + tickadj_upper + tickadj_extra);
}

/* tickadj_upper is in units of "ticks per interrupt". Since the timer runs
 * at 2MHz, 1 tick = 500ns, and since there are 8 interrupts per second, 1
 * interrupt is nominally 62,500 ticks. One unit of tickadj_upper is thus
 * 1/62500 = 16ppm. tickadj_lower is in "sub-tick units"; 256 tickadj_lower
 * units equal one tickadj_upper unit. One tickadj_lower unit is thus 1/16 ppm.
 * The dithering is done simply by maintaining an accumulator and adding
 * tickadj_lower to it on every interrupt; whenever the accumulator overflows,
 * one extra tick (tickadj_extra) is added to the timer interval.
 */

void tickadj_run() {
  unsigned char old_accum = tickadj_accum;
  tickadj_accum += tickadj_lower;
  if (tickadj_accum < old_accum) {
    tickadj_extra = 1;
  } else {
    tickadj_extra = 0;
  }
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
static int32 pps_history[5] = { 0L, 0L, 0L, 0L, 0L };
static short last_slew_rate = 0;
static int32 ppschange_int;
static char lasthardslew = 0;
static int32 slew_accum = 0;
static char startup = 2;

static short clocks = 0;

#define PLL_SLEW_DIV 1024L
#define PLL_SLEW_MAX 8192L
#define PLL_SLEW_SLOW_ZONE 10
#define PLL_RATE_DIV 2048L
#define PLL_SKEW_MAX 32

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

  debug("PPS: "); debug_long(pps_ns_copy);

  pps_history[4] = pps_history[3];
  pps_history[3] = pps_history[2];
  pps_history[2] = pps_history[1];
  pps_history[1] = pps_history[0];
  pps_history[0] = pps_ns_copy;

  int32 pps_filtered = med_mean_filter(pps_history);
//  int32 pps_filtered = median_filter(pps_history);
  debug(" ("); debug_long(pps_filtered); debug(")");
  debug("\n");

  short slew_rate = 0;
  char hardslew = 0;

  if ((pps_ns_copy < -31250000L && pps_filtered < -31250000L) || (pps_ns_copy > 31250000L && pps_filtered > 31250000L)) {
    ints -= pps_ns_copy / 31250000L;
    hardslew = pps_ns_copy / 31250000L;
    ppschange_int = 0;
    slew_accum = 0;
    clocks = 0;
  } else {
    slew_accum += pps_filtered;
  }

  if (slew_accum >= PLL_SLEW_DIV || slew_accum <= -PLL_SLEW_DIV) {
    if (slew_accum >= (PLL_SLEW_MAX + PLL_SLEW_SLOW_ZONE / 2) * PLL_SLEW_DIV) {
      slew_rate = PLL_SLEW_MAX;
      slew_accum -= PLL_SLEW_MAX * PLL_SLEW_DIV;
      slew_accum /= 2;
    } else if (slew_accum <= -(PLL_SLEW_MAX + PLL_SLEW_SLOW_ZONE / 2) * PLL_SLEW_DIV) {
      slew_rate = -PLL_SLEW_MAX;
      slew_accum += PLL_SLEW_MAX * PLL_SLEW_DIV;
      slew_accum /= 2;
    } else {
      slew_rate = slew_accum / PLL_SLEW_DIV;
      if (slew_rate > PLL_SLEW_SLOW_ZONE) {
        slew_rate -= PLL_SLEW_SLOW_ZONE / 2;
      } else if (slew_rate < -PLL_SLEW_SLOW_ZONE) {
        slew_rate += PLL_SLEW_SLOW_ZONE / 2;
      } else if (slew_rate <= -2 || slew_rate >= -2) {
        slew_rate /= 2;
      }
      slew_accum -= slew_rate * PLL_SLEW_DIV;
    }
  }

  if (!hardslew && (lasthardslew || startup)) {
    int32 ppschange = pps_ns_copy - pps_history[1] + 
      (int32)lasthardslew * 31250000L;
    /* 62.5 ns per clock */
    clocks = (ppschange * 2) / 125;
    if (startup) startup--;
  } else if (!hardslew) {
    /* The ideal factor for last_slew_rate here is 62.5 (1000 / 16), so the
     * choices are 62 and 63. 63 gives slightly faster lock-on, but carries a
     * risk that the PLL will get stuck in a state where slew != 0, so we
     * use 62 here.
     */
    int32 ppschange = pps_ns_copy - pps_history[1] + (int32)last_slew_rate * 62;
    // debug("PPS change raw: "); debug_long(ppschange); debug("\n");
    ppschange_int += ppschange;
    // debug("PPS change integrated: "); debug_long(ppschange_int); debug("\n");
    if (ppschange_int < -PLL_RATE_DIV) {
      if (ppschange_int < -PLL_SKEW_MAX * PLL_RATE_DIV) {
        // debug("Speed ++\n");
        clocks -= PLL_SKEW_MAX;
        ppschange_int += PLL_RATE_DIV * PLL_SKEW_MAX;
        ppschange_int /= 2;
      } else {
        // debug("Speed +\n");
        clocks += ppschange_int / PLL_RATE_DIV;
        ppschange_int -= PLL_RATE_DIV * (ppschange_int / PLL_RATE_DIV);
      }
    } else if (ppschange_int > PLL_RATE_DIV) {
      if (ppschange_int > PLL_SKEW_MAX * PLL_RATE_DIV) {
        // debug("Speed --\n");
        clocks += PLL_SKEW_MAX;
        ppschange_int -= PLL_RATE_DIV * PLL_SKEW_MAX;
        ppschange_int /= 2;
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

  debug("PLL: "); debug_int(clocks);
  debug(" ");
  if (slew_rate >= 0) debug("+"); debug_int(slew_rate);
  debug(" = ");
  debug_int(clocks + slew_rate);
  debug("\n");
  debug("Temp: "); debug_float(tempprobe_gettemp()); debug("\n");

  tickadj_set_clocks(clocks + slew_rate);
}
