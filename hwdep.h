#ifndef __HWDEP_H
#define __HWDEP_H

#ifdef SIMULATE

#include <stdio.h>

#define CPU_CLOCK 16000000L

#define debug(x) printf("%s", x)
#define debug_int(x) printf("%d", x)
#define debug_long(x) printf("%ld", x)
#define debug_float(x) printf("%f", x)

typedef int int32;
typedef unsigned int uint32;

struct _timer_struct {
  unsigned char prediv;
  unsigned char prediv_count;
  unsigned short top;
  unsigned short counter;
};

extern struct _timer_struct timer;

extern void timer_clk();
extern void gps_clk();

inline unsigned short timer_get_counter () {
  return timer.counter;
}

#else

#include "WProgram.h"
#include "HardwareSerial.h"

#define CPU_CLOCK 16000000L

#if 1
#define debug(x) Serial.print(x)
#define debug_int(x) Serial.print(x)
#define debug_long(x) Serial.print(x)
#define debug_float(x) Serial.print(x)
#else
#define debug(x) do { } while(0)
#define debug_int(x) do { } while(0)
#define debug_long(x) do { } while(0)
#define debug_float(x) do { } while(0)
#endif

typedef long int int32;
typedef unsigned long int uint32;

inline unsigned short timer_get_counter () {
  return TCNT4;
}

#endif

#define DEF_TIMER_VAL 62500
#define MAX_TIMER_VAL 65535

#define PREDIV 8
#define NS_PER_COUNT (1000000000L / (CPU_CLOCK / PREDIV))
#define NS_PER_INT (1000000000L / ((CPU_CLOCK / PREDIV) / DEF_TIMER_VAL))
#define INT_PER_SEC ((CPU_CLOCK / PREDIV) / DEF_TIMER_VAL)

extern uint32 time_get_ns();
extern void timer_init();
extern void timer_set_interval(unsigned short);

extern char sec;
static void print_time() {
  uint32 ns = time_get_ns();
  debug_int(sec); debug("."); debug_long(ns); debug("\n");
}

#endif

