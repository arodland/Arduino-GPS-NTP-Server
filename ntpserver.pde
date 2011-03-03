#include "hwdep.h"
#include "timing.h"
#include "gps.h"

volatile extern char pps_int;

void setup () {
  pinMode(13, OUTPUT);
  for (int k = 0 ; k < 3 ; k++) {
    digitalWrite(13, HIGH);
    delay(250);
    digitalWrite(13, LOW);
    delay(250);
  }
  Serial.begin(115200);
  timer_init();
//  tickadj_set_clocks(-3552); /* +222 ppm */
//  tickadj_set_clocks(0);
  gps_init();
}

void loop () {
#if 0
  if (Serial.available()) {
    int chin = Serial.read();
    Serial1.print(chin, BYTE);
  }

  if (Serial1.available()) {
    int chin = Serial1.read();
    Serial.print(chin, BYTE);
    if (chin == '\x0a') {
#endif
      if (pps_int) {
        pll_run();
      }
      gps_poll();
#if 0
    }
  }
#endif
}

void second_int() {
//  print_time();
}

// vim: ft=cpp
