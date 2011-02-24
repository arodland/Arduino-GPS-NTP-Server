#include "hwdep.h"
#include "timing.h"

volatile static int32 i;

void second_int() {
  debug_long(time_get_ns()); debug("\n");
}

void gps_int() {
  uint32 ns = time_get_ns();
  debug("Got GPS int, ns="); debug_long(ns); debug("\n");
}

int main() {
#ifndef SIMULATE
  init();
  pinMode(13, OUTPUT);
  for (int k = 0 ; k < 3 ; k++) {
    digitalWrite(13, HIGH);
    delay(250);
    digitalWrite(13, LOW);
    delay(250);
  }
  Serial.begin(115200);
#endif

  timer_init();

  tickadj_set_ppm(209);

  debug("NS_PER_COUNT="); debug_long(NS_PER_COUNT); debug("\n");
  debug("NS_PER_INT="); debug_long(NS_PER_INT); debug("\n");
  debug("INT_PER_SEC="); debug_long(INT_PER_SEC); debug("\n");

#ifdef SIMULATE
  for (i = 0 ; i <= 32100000 ; i++) {
    timer_clk();
  }
#else
  for (;;) {
    i++;
  }
#endif
}

