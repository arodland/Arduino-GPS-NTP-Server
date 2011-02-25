#include "hwdep.h"
#include "timing.h"

volatile extern char pps_int;
volatile extern uint32 pps_ns;

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
  tickadj_set_clocks(-3552); /* +222 ppm */
  Serial1.begin(4800);
}

void loop () {
  if (Serial1.available()) {
    int chin = Serial1.read();
    Serial.print(chin, BYTE);
    if (chin == '\n') {
      if (pps_int) {
        pps_int = 0;
        Serial.print("PPS: ");
        Serial.println(pps_ns);
      }
    }
  }
}

void second_int() {
//  print_time();
}

// vim: ft=cpp
