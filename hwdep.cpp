#include "hwdep.h"
#include "gps.h"
#include "lcd.h"

char pps_int = 0;

#ifdef SIMULATE

#include <stdlib.h>

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
}

unsigned short timer_get_interval() {
  return timer.top + 1;
}

extern void timer_int();

void gps_init() {
  return; /* XXX unimplemented */
}
#define GPS_CYCLES (32000000L - 2007L)
//#define GPS_CYCLES (32000000L - 3L)

static uint32 gps_clk = GPS_CYCLES / 64;

void sim_clk() {
  timer.prediv_count ++;
  if (timer.prediv_count == timer.prediv) {
    timer.prediv_count = 0;
    timer.counter++;
    if (timer.counter > timer.top) {
      timer.counter = 0;
      timer_int();
    }
  }
  gps_clk += 2;
  if (gps_clk >= GPS_CYCLES) {
    gps_clk -= GPS_CYCLES;
#ifdef LCD
    lcd_set_displaydate(2012, 7, 4, 12, 34, 56, -15);
    lcd_set_gps_status(4);
#endif

    timer.capture = timer.counter;
    time_get_ns_capt();
    pps_int = 1;
  }
}

#else

static volatile int timer_ready = 0;

extern void timer_int();

void int4();

void timer_init() {
  pinMode(49, INPUT);
  pinMode(2, INPUT);

//  TCCR4A = _BV(COM4A1) | _BV(COM4A0);
//  TCCR4A = 0;
  TCCR4A = 0;
  PORTH &= ~_BV(PORTH4);

  TCCR4B = _BV(CS41) | _BV(WGM42) | _BV(ICES4);

  TIMSK4 = _BV(OCIE4A) | _BV(ICIE4) | _BV(TOIE4);

  timer_set_interval(DEF_TIMER_VAL);
  timer_ready = 1;
}

ISR(TIMER4_COMPA_vect) {
  TIFR4 |= _BV(ICF4);

  if (timer_ready)
    timer_int();
}

ISR(TIMER4_OVF_vect) {
  if (timer_ready) {
    Serial.println("OVF");
    /* Handle an overrun nicely */
  }
}

ISR(TIMER4_CAPT_vect) {
  time_get_ns_capt();
  pps_int = 1;
}

void timer_set_interval(unsigned short top) {
  OCR4A = top - 1;
}

unsigned short timer_get_interval() {
  return OCR4A + 1;
}

void gps_set_nmea_reporting();

void gps_init() {
  GPSPORT.begin(4800);
//  gps_set_nmea_reporting();
  gps_set_sirf();
  gps_enable_dgps();
}

#endif
