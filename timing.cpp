#include "hwdep.h"

volatile static char ints = 0;
/* static */ char sec = 0;
static signed char tickadj_upper = 0;
static unsigned char tickadj_lower = 0;
static unsigned char tickadj_phase = 0;

uint32 make_ns(unsigned char ints, unsigned short counter) {
  return ints * NS_PER_INT + counter * NS_PER_COUNT;
}

uint32 time_get_ns() {
  return make_ns(ints, timer_get_counter());
}

void tickadj_adjust() {
  int x = (tickadj_phase / 16 < tickadj_lower / 16) ? 1 : 0;
  int y = (tickadj_phase % 16 < (tickadj_lower % 16) + x);

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

static int ledstate = 1;

void timer_int() {
  ints++;
  if (ints == INT_PER_SEC) {
    sec++;
    ints = 0;
    second_int();
  }

#ifndef SIMULATE
  ledstate = digitalRead(13);
  ledstate = ledstate ? 0 : 1;
  digitalWrite(13, ledstate);  
#endif

  tickadj_run();
}

void tickadj_set(signed char upper, unsigned char lower) {
  tickadj_upper = upper;
  tickadj_lower = lower;
  debug("tickadj_upper = "); debug_int(tickadj_upper);
  debug(", tickadj_lower = "); debug_int((short)tickadj_lower); debug("\n");
  tickadj_adjust();
}

/* Positive PPM will make the clock run fast, negative slow */
void tickadj_set_ppm(signed short ppm) {
  /* Negation is a shortcut for computing 1 / ((1M + ppm) / 1M) that's accurate
   * out to a few hundred, which is all we want. From her on out the clock speed
   * will actually be *divided* by (1000000 + ppm) / 10000000
   */

  ppm = -ppm;
  signed short upper = ppm  / 16;
  unsigned char lower = ppm % 16;

  if (ppm < 0) {
    upper--; /* lower is always interpreted as an addition, so e.g. -10/256
                will come up here as lower=246, gross=0. We need to make it
                gross=-1 so that -1 + 246/256 == -10/256
              */
    lower--;
  }

  if (upper < -128 || upper > 127) {
    debug("time adjustment out of range!\n");
  } else {
    tickadj_set(upper, lower);
  }
}

