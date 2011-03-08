#ifndef __HWDEP_H
#define __HWDEP_H


#ifdef SIMULATE

#include <stdio.h>

#define debug(x) printf("%s", x)
#define debug_int(x) printf("%d", x)
#define debug_long(x) printf("%d", (signed int)x)
#define debug_ulong(x) printf("%u", (unsigned int)x)
#define debug_float(x) printf("%f", x)

typedef signed int int32;
typedef unsigned int uint32;

struct _timer_struct {
  unsigned char prediv;
  unsigned char prediv_count;
  unsigned short top;
  unsigned short counter;
  unsigned short capture;
};

extern struct _timer_struct timer;

extern void sim_clk();

inline unsigned short timer_get_counter () {
  return timer.counter;
}

inline unsigned short timer_get_capture () {
  return timer.capture;
}

inline char timer_get_pending () {
  return 0;
}

inline int gps_can_read() {
  return 0; /* XXX not implemented */
}

inline int gps_read() {
  return -1; /* XXX not implemented */
}

inline void gps_write(const char * data) {
  return; /* XXX not implemented */
}

inline void gps_writebyte(const char ch) {
  return; /* XXX not implemented */
}

inline void gps_set_baud(int baud) {
  return; /* Does nothing */
}

inline void delay(int millis) {
  return; /* Does nothing */
}

#else

#include "WProgram.h"
#include "HardwareSerial.h"

#if 0
#include <Ethernet.h>
extern Server debugserver;
extern Client debugclient;
#define debugsocket_printif(x) do {\
  if (debugclient.connected())\
    debugserver.print(x);\
} while (0)
#define debug(x) debugsocket_printif(x)
#define debug_int(x) debugsocket_printif(x)
#define debug_long(x) debugsocket_printif(x)
#define debug_float(x) debugsocket_printif(x)
#else
#define debug(x) Serial.print(x)
#define debug_int(x) Serial.print(x)
#define debug_long(x) Serial.print(x)
#define debug_float(x) Serial.print(x, 2)
#endif

typedef long int int32;
typedef unsigned long int uint32;

inline unsigned short timer_get_counter () {
  return TCNT4;
}

inline unsigned short timer_get_capture () {
  return ICR4;
}

inline char timer_get_pending () {
  return ((TIFR4 & _BV(OCF4A)) ? 1 : 0);
}

#define GPSPORT Serial1

inline int gps_can_read() {
  return GPSPORT.available();
}

inline int gps_read() {
  return GPSPORT.read();
}

inline void gps_write(const char *data) {
  Serial.print(data);
  GPSPORT.print(data);
}

inline void gps_writebyte(const char ch) {
  GPSPORT.print(ch, BYTE);
}

inline void gps_set_baud(long baud) {
  GPSPORT.flush();
  delay(500);
  GPSPORT.begin(baud);
  GPSPORT.flush();
}


#endif

#include "timing.h"

const uint32 CPU_CLOCK = F_CPU; /* Defined by -DF_CPU */
const unsigned int DEF_TIMER_VAL = 62500;
const unsigned int MAX_TIMER_VAL = 65535;

const unsigned int PREDIV = 8;

const uint32 NS_PER_COUNT = (1000000000UL / (CPU_CLOCK / PREDIV));
const unsigned int INT_PER_SEC = ((CPU_CLOCK / PREDIV) / DEF_TIMER_VAL);

// adding tm/2 here biases by 1/2 so it rounds :)
inline uint32 NSPC(unsigned int tm) {
  return ((tm / 2 + NS_PER_COUNT * DEF_TIMER_VAL) / tm);
}

inline uint32 NSPI(unsigned int tm) {
  return (1000000000UL / ((CPU_CLOCK / PREDIV) / tm));
}

const uint32 NS_PER_INT = (1000000000UL / ((CPU_CLOCK / PREDIV) / DEF_TIMER_VAL));

const uint32 NTP_PER_COUNT = (0x100000000ULL / (CPU_CLOCK / PREDIV));

inline uint32 NTPPC(unsigned int tm) {
  return ((tm / 2 + NTP_PER_COUNT * DEF_TIMER_VAL) / tm);
}

inline uint32 NTPPI(unsigned int tm) {
  return (0x100000000ULL / ((CPU_CLOCK / PREDIV) / tm));
}

const uint32 NTP_PER_INT = (0x100000000ULL / ((CPU_CLOCK / PREDIV) / DEF_TIMER_VAL));

extern void timer_init();
extern void timer_set_interval(unsigned short);
extern unsigned short timer_get_interval();

#endif

