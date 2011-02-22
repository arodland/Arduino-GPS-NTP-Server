#include <stdio.h>
#include <stdlib.h>

#define CPU_CLOCK 16000000L

#define DEF_TIMER_VAL 62500
#define MAX_TIMER_VAL 65535

#define PREDIV 8
#define NS_PER_COUNT (1000000000L / (CPU_CLOCK / PREDIV))
#define NS_PER_INT (1000000000L / ((CPU_CLOCK / PREDIV) / DEF_TIMER_VAL))
#define INT_PER_SEC ((CPU_CLOCK / PREDIV) / DEF_TIMER_VAL)

#define debug(...) printf(__VA_ARGS__)

struct {
  unsigned char prediv;
  unsigned char prediv_count;
  unsigned short top;
  unsigned short counter;
} timer;

/* Set up for 1:1 divisor and 1 interrupt per 65536 */
void timer_init() {
  timer.counter = 0;
  timer.top = MAX_TIMER_VAL;
  timer.prediv_count = 0;
  timer.prediv = 1;
}

void timer_int();

void timer_clk() {
  timer.prediv_count ++;
  if (timer.prediv_count == timer.prediv) {
    timer.prediv_count = 0;
    timer.counter++;
    if (timer.counter > timer.top) {
      timer.counter = 0;
      timer_int();
    }
  }
}

void timer_set_interval(unsigned short top) {
  timer.top = top - 1;
  debug("New timer period %d\n", timer.top);
}

static char ints = 0;
static char sec = 0;
static signed char tickadj_upper = 0;
static unsigned char tickadj_lower = 0;
static unsigned char tickadj_phase = 0;

unsigned long time_get_ns() {
  return ints * NS_PER_INT + timer.counter * NS_PER_COUNT;
}

void print_time() {
  unsigned long ns = time_get_ns();
  debug("%d.%09ld\n", sec, ns);
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
  debug("PLL phase %d\n", tickadj_phase);
  tickadj_adjust();
}

void second_int();

void timer_int() {
  ints++;
  debug("Interrupt %d!\n", ints);
  if (ints == INT_PER_SEC) {
    sec++;
    ints = 0;
    second_int();
  }

  tickadj_run();
  print_time();
}

void tickadj_set(signed char upper, unsigned char lower) {
  tickadj_upper = upper;
  tickadj_lower = lower;
  debug("tickadj_upper = %d, tickadj_lower = %d\n", tickadj_upper, tickadj_lower);
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
    exit(1);
  }

  tickadj_set(upper, lower);
}

static int i = 0;

void second_int() {
  debug("i = %d\n", i);
}

void main() {
  timer_init();
  timer.prediv = PREDIV;
  timer.top = DEF_TIMER_VAL - 1;

  tickadj_set_ppm(1);

  debug("NS_PER_COUNT=%ld\nNS_PER_INT=%ld\nINT_PER_SEC=%ld\n",
      NS_PER_COUNT, NS_PER_INT, INT_PER_SEC);


  for (i = 0 ; i <= 32100000 ; i++) {
    timer_clk();
  }
}

