#include "hwdep.h"
#include "timing.h"

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
  Serial1.begin(38400);
//  Serial1.print("\xa0\xa2\x00\x18\x81\x02\x01\x01\x00\x01\x01\x01\x05\x01\x01\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x25\x80\x01\x3a\xb0\xb3");
/*  char msg[] = "\xa0\xa2\x00\x18\x81\x02\x01\x01\x00\x00\x01\x01\x05\x01\x01\x01\x00\x00\x00\x01\x00\x01\x00\x01\x00\x01\x96\x00\x01\x29\xb0\xb3";
  for (int i = 0 ; i < sizeof(msg) ; i++) {
    Serial1.print(msg[i], BYTE);
  } */
//  Serial1.print("$PSRF100,1,38400,8,1,0*3C\x0d\x0a");

/*  Serial1.print("$PSRF103,00,00,01,01*25\x0d\x0a"); // GPGGA 1sec
  Serial1.print("$PSRF103,01,00,01,01*24\x0d\x0a"); // GPGLL off
  Serial1.print("$PSRF103,02,00,01,01*27\x0d\x0a"); // GPGSA 1sec
  Serial1.print("$PSRF103,03,00,10,01*26\x0d\x0a"); // GPGSV 10sec
  Serial1.print("$PSRF103,04,00,01,01*21\x0d\x0a"); // GPRMC 1sec
  Serial1.print("$PSRF103,05,00,00,01*21\x0d\x0a"); // GPVTG off
  Serial1.print("$PSRF103,07,00,01,01*22\x0d\x0a"); // GPZDA 1sec
  Serial1.print("$PSRF151,01*0F\x0d\x0a"); // Enable WAAS */
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
#if 0
    }
  }
#endif
}

void second_int() {
//  print_time();
}

// vim: ft=cpp
