#include "hwdep.h"
#include "timing.h"

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
}

static int i = 0;

void loop () {
  i++;
}

void second_int() {
  print_time();
}
