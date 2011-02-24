#include "hwdep.h"

#ifdef SIMULATE

struct _timer_struct timer;

/* Set up for 1:1 divisor and 1 interrupt per 65536 */
void timer_init() {
  timer.counter = 0;
  timer.top = MAX_TIMER_VAL;
  timer.prediv_count = 0;
  timer.prediv = PREDIV;
}

void timer_set_interval(unsigned short top) {
  timer.top = top - 1;
  debug("period "); debug_int(timer.top); debug("\n");
}

unsigned short timer_get_interval() {
  return timer.top + 1;
}

extern void timer_int();

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

extern void gps_int();

static int gps_counter = 0;

#define GPS_RATE 16000128L

void gps_clk() {
  gps_counter++;
  if (gps_counter == GPS_RATE) {
    gps_counter = 0;
    gps_int();
  }
}

#else

static volatile int timer_ready = 0;

extern void timer_int();

#define TIMER4_COMPA_INT 42

void timer_init() {
  TCCR4A = 0;
  bitWrite(TCCR4A, COM4A1, 1);
  bitWrite(TCCR4A, COM4A0, 1);
  
  TCCR4B = 0;
  bitWrite(TCCR4B, CS41, 1);
  bitWrite(TCCR4B, WGM42, 1);
  TIMSK4 = 0;
  bitWrite(TIMSK4, OCIE4A, 1);
  timer_set_interval(DEF_TIMER_VAL);
  timer_ready = 1;
}

ISR(TIMER4_COMPA_vect) {
  if (timer_ready) 
    timer_int();
}

ISR(TIMER4_OVF_vect) {
  if (timer_ready) {
    /* Handle an overrun nicely */
  }
}

void timer_set_interval(unsigned short top) {
  OCR4A = top;
}
  
unsigned short timer_get_interval() {
  return OCR4A;
}

#endif
