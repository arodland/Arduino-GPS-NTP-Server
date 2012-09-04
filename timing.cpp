#include "config.h"
#include "hwdep.h"
#include "tempprobe.h"
#include "lcd.h"

#include <string.h>

volatile /* static */ char ints = 0;
/* For timing medium-resolution events like tempprobe reading and DHCP
 * renewal that shouldn't run with interrupts disabled
 */
volatile char schedule_ints = 0;
static signed char tickadj_upper = 0;
static unsigned short tickadj_lower = 0;
static unsigned short tickadj_accum = 0;
static unsigned char tickadj_extra = 0;

static short fll_slot = 0;
static short fll_valid = 0;
static uint32 fll_history[64];
static uint32 fll_counter = 0;

static unsigned int gps_week = 0;
static uint32 tow_sec_utc = 0;

volatile extern char ints;
volatile char pps_ints = 0;
volatile unsigned short pps_timer = 0;
volatile extern char pps_int;
volatile uint32 pps_ns;

void time_set_date(unsigned int week, uint32 gps_tow, int offset) {
  if ((int32)gps_tow + offset < 0) {
    tow_sec_utc = gps_tow + 604800L + offset;
    gps_week = week - 1;
  } else {
    tow_sec_utc = gps_tow + offset;
    gps_week = week;
  }
}

const uint32 PLL_OFFSET_NS = 15625000L - PLL_FUDGE_NS;
extern const int32 NTP_FUDGE_RX = PLL_OFFSET_NTP + (NTP_FUDGE_RX_US * 429497) / 100;
extern const int32 NTP_FUDGE_TX = PLL_OFFSET_NTP + (NTP_FUDGE_TX_US * 429497) / 100;

static inline uint32 sawtooth_ns(unsigned short tm) {
  uint32 ns = (NSPC(tm) * tickadj_accum + 1024) >> 11;
  return ns;
}

int32 make_ns(unsigned char i, unsigned short counter) {
  unsigned short tm = timer_get_interval();
  int32 ns = i * NS_PER_INT + counter * NSPC(tm);
#ifdef SAWTOOTH_COMP
  ns -= sawtooth_ns(tm) + NSPC(tm) * tickadj_extra;
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
#ifdef SAWTOOTH_COMP
  ns -= sawtooth_ns(tm) + NSPC(tm) * tickadj_extra;
#endif
  if (ns + PLL_OFFSET_NS > 1000000000L) {
    ns -= 1000000000L;
    *add_sec = 1;
  } else
    *add_sec = 0;
  ns += PLL_OFFSET_NS;
  return ns;
}

uint32 make_ntp(unsigned char i, unsigned short counter, int32 fudge) {
  unsigned short tm = timer_get_interval();
  uint32 ntp = i * NTP_PER_INT + counter * NTPPC(tm);
#ifdef SAWTOOTH_COMP
  ntp -= tickadj_accum + NTPPC(tm) * tickadj_extra;
#endif
  ntp += fudge;
  return ntp;
}

uint32 make_ntp_carry(unsigned char i, unsigned short counter, int32 fudge, char *add_sec) {
  unsigned short tm = timer_get_interval();
  uint32 ntp = i * NTP_PER_INT + counter * NTPPC(tm);
#ifdef SAWTOOTH_COMP
  ntp -= tickadj_accum + NTPPC(tm) * tickadj_extra;
#endif
  uint32 ntp_augmented = ntp + fudge;
  *add_sec = ntp_augmented < ntp;
  return ntp_augmented;
}

int32 time_get_ns() {
  char i = ints + timer_get_pending();
  unsigned short ctr = timer_get_counter();
  return make_ns(i, ctr);
}

uint32 time_get_ntp_lower(int32 fudge) {
  char i = ints + timer_get_pending();
  unsigned short ctr = timer_get_counter();
  return make_ntp(i, ctr, fudge);
}

void time_get_ntp(uint32 *upper, uint32 *lower, int32 fudge) {
  char add_sec;
  *upper = 2524953600UL; /* GPS epoch - NTP epoch */
  *upper += gps_week * 604800UL; /* 1 week */
  *upper += tow_sec_utc;

  char i = ints + timer_get_pending();
  unsigned short ctr = timer_get_counter();
  *lower = make_ntp_carry(i, ctr, fudge, &add_sec);
  *upper += add_sec;
}

void time_get_ns_capt() {
  pps_ints = ints;
  pps_timer = timer_get_capture();
  if (pps_timer > timer_get_counter()) { /* Timer reset after capture */
    pps_ints --;
  }
  pps_ns = make_ns(pps_ints, pps_timer);
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
 * at 2MHz, 1 tick = 500ns, and since there are 32 interrupts per second, 1
 * interrupt is nominally 62,500 ticks. One unit of tickadj_upper is thus
 * 1/62500 = 16ppm. tickadj_lower is in "sub-tick units"; 2048 tickadj_lower
 * units equal one tickadj_upper unit. One tickadj_lower unit is thus 1/128 ppm.
 * The dithering is done simply by maintaining an accumulator and adding
 * tickadj_lower to it on every interrupt; whenever the accumulator overflows,
 * one extra tick (tickadj_extra) is added to the timer interval.
 */

void tickadj_run() {
  tickadj_accum += tickadj_lower;
  if (tickadj_accum >= 2048) {
    tickadj_extra = 1;
    tickadj_accum -= 2048;
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

void timer_int() {
  ints++;
  if (ints == INT_PER_SEC) {
    ints = 0;
    second_int();
  }
#ifndef SIMULATE
  switch (ints) {
    case (INT_PER_SEC - 1):
      /* Drive PPS high 1/64 sec after int 0 */
      OCR4B = OCR4A / 2;
      TCCR4A = _BV(COM4B1) | _BV(COM4B0);
      break;
    case 0:
      PORTB |= _BV(PORTB7); /* Turn on LED */
      break;
    case 3:
      /* Drive PPS low 1/64 sec after int 4 */
      TCCR4A = _BV(COM4B1);
      PORTB &= ~_BV(PORTB7); /* Turn off the LED */
      break;
  }
#endif
  fll_counter += timer_get_interval();

  tickadj_run();

  schedule_ints++;
}

void tickadj_set(signed char upper, unsigned short lower) {
  tickadj_upper = upper;
  tickadj_lower = lower;
//  debug("tickadj_upper = "); debug_int(tickadj_upper);
//  debug(", tickadj_lower = "); debug_int((short)tickadj_lower); debug("\n");
  tickadj_adjust();
}

void tickadj_set_clocks(int32 clocks) {
  signed char upper = clocks / 2048;
  unsigned short lower = clocks % 2048;

  if (clocks < 0) {
    upper --;
    lower += 2048;
  }

  tickadj_set(upper, lower);
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



extern int tempprobe_corr;

static int32 pps_ns_copy = 0;
static int32 pps_history[5] = { 0L, 0L, 0L, 0L, 0L };
static int32 ppschange_history[4] = { 0L, 0L, 0L, 0L };
static int32 last_pps_filtered = 0;
static short last_slew_rate = 0;
static char lasthardslew = 0;
static int32 slew_accum = 0;

static short clocks = 0;
static short clocks_dither = 0;
static short cda = 0;

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

  int32 pps_filtered = median_filter(pps_history);
//  int32 pps_filtered = median_filter(pps_history);
  debug(" ("); debug_long(pps_filtered); debug(")");

#ifdef SAWTOOTH_COMP
  debug(" Sawtooth: "); debug_long(sawtooth_ns(timer_get_interval()));
#endif

  debug("\n");

  short slew_rate = 0;
  char hardslew = 0;

  if ((pps_ns_copy < -31250000L && pps_filtered < -31250000L) || (pps_ns_copy > 31250000L && pps_filtered > 31250000L)) {
    ints -= pps_ns_copy / 31250000L;
    hardslew = pps_ns_copy / 31250000L;
    fll_slot = 0;
    fll_valid = 0;
    fll_counter = 0;
    slew_accum = 0;
    clocks = 0;
  } else {
    uint32 fll = fll_counter + pps_timer;
    if (pps_ints < ints) {
      fll -= timer_get_interval();
    }

    if (fll_valid < 64) {
      if (fll_valid > 1) {
        clocks = ((fll - fll_history[(fll_slot - 1) % 64]) - 2000000L) * 64;
      }
      fll_valid ++;
    } else {
      clocks = (fll - fll_history[fll_slot]) - 128000000L;
    }
    fll_history[fll_slot] = fll;
    fll_slot = (fll_slot + 1) % 64;

    slew_accum += pps_filtered;
  }

  if (slew_accum >= PLL_SLEW_DIV || slew_accum <= -PLL_SLEW_DIV) {
    if (slew_accum >= (PLL_SLEW_MAX + PLL_SLEW_SLOW_ZONE / 2) * PLL_SLEW_DIV) {
      slew_rate = PLL_SLEW_MAX;
      slew_accum = 0L - PLL_SLEW_MAX * PLL_SLEW_DIV;
    } else if (slew_accum <= -(PLL_SLEW_MAX + PLL_SLEW_SLOW_ZONE / 2) * PLL_SLEW_DIV) {
      slew_rate = -PLL_SLEW_MAX;
      slew_accum = PLL_SLEW_MAX * PLL_SLEW_DIV;
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

  last_pps_filtered = pps_filtered;
  last_slew_rate = slew_rate;
  lasthardslew = hardslew;

  debug("PLL: "); debug_long(clocks);
  debug(" ");
  if (slew_rate >= 0) debug("+"); debug_int(slew_rate);
#ifdef TEMPCORR
  debug(" ");
  if (tempprobe_corr >= 0) debug("+"); debug_int(tempprobe_corr);
#endif
  debug(" = ");
  debug_int(clocks + slew_rate + tempprobe_corr);
  debug("\n");
#ifdef TEMPCORR
  debug("Temp: "); debug_float(tempprobe_gettemp()); debug("\n");
#endif

#ifdef LCD
  lcd_set_pll_status(pps_ns_copy, clocks + slew_rate + tempprobe_corr);
  lcd_draw();
#endif
  tickadj_set_clocks(clocks + slew_rate + tempprobe_corr);
}
