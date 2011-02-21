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
static signed char plladj_major = 0;
static unsigned char plladj_minor = 0;
static unsigned char plladj_phase = 0;

void print_time();

void adjust_pll() {
  int x = (plladj_phase / 16 < plladj_minor / 16) ? 1 : 0;
  int y = (plladj_phase % 16 < (plladj_minor % 16) + x);

  /* these actually give periods of DEF_TIMER_VAL + plladj_major and
   * DEF_TIMER_VAL + plladj_major + 1 because the timer adds one. This means
   * the avg. period is (DEF_TIMER_VAL + plladj_major + plladj_minor / 256)
   * and the frequency is 2MHz / that.
   */
  if (y) {
    timer_set_interval(DEF_TIMER_VAL + plladj_major + 1);
  } else {
    timer_set_interval(DEF_TIMER_VAL + plladj_major);
  }
}

void timer_int() {
  ints++;
  debug("Interrupt %d!\n", ints);
  if (ints == INT_PER_SEC) {
    sec++;
    ints = 0;
  }

  plladj_phase++;
  debug("PLL phase %d\n", plladj_phase);
  adjust_pll();

  print_time();
}

void print_time() {
  unsigned long ns = ints * NS_PER_INT + timer.counter * NS_PER_COUNT;
  debug("%d.%09ld\n", sec, ns);
}

/* Positive PPM will make the clock run fast, negative slow */
void time_set_adj(signed short ppm) {
  /* Negation is a shortcut for computing 1 / ((1M + ppm) / 1M) that's accurate
   * out to a few hundred, which is all we want. From her on out the clock speed
   * will actually be *divided* by (1000000 + ppm) / 10000000
   */

  ppm = -ppm;
  signed short gross = ppm  / 16;
  unsigned char lower = ppm % 16;

  if (ppm < 0) {
    gross--; /* lower is always interpreted as an addition, so e.g. -10/256
                will come up here as lower=246, gross=0. We need to make it
                gross=-1 so that -1 + 246/256 == -10/256
              */
    lower--;
  }

  if (gross < -128 || gross > 127) {
    debug("time adjustment out of range!\n");
    exit(1);
  }

  plladj_major = gross;
  plladj_minor = lower;

  debug("plladj_major = %d, plladj_minor = %d\n", plladj_major, plladj_minor);
  adjust_pll();
}

void main() {
  timer_init();
  timer.prediv = PREDIV;
  timer.top = DEF_TIMER_VAL - 1;

  time_set_adj(50);

  debug("NS_PER_COUNT=%ld\nNS_PER_INT=%ld\nINT_PER_SEC=%ld\n",
      NS_PER_COUNT, NS_PER_INT, INT_PER_SEC);


  for (int i = 0 ; i <= 32100000 ; i++) {
    debug("i = %d\n", i);
    timer_clk();
  }
}

