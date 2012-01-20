#include <avr/power.h>
#include <avr/sleep.h>

#include "hwdep.h"
#include "timing.h"
#include "gps.h"
#include "ethernet.h"
#include "tempprobe.h"

volatile extern char pps_int;
volatile extern char schedule_ints;

void setup () {
  power_adc_disable();
  power_twi_disable();
  power_usart2_disable();
  power_usart3_disable();
  pinMode(13, OUTPUT);
  for (int k = 0 ; k < 3 ; k++) {
    digitalWrite(13, HIGH);
    delay(250);
    digitalWrite(13, LOW);
    delay(250);
  }
  Serial.begin(115200);
  timer_init();
  gps_init();
  ether_init();
  tempprobe_init();
}

void loop () {
  while (1) {
    if (pps_int) {
      pll_run();
    }
    ether_poll();
    gps_poll();
    ether_poll();
    while (schedule_ints) {
      tempprobe_run();
      ether_dhcp_poll();
      schedule_ints--;
    }
    sleep_mode();
  }
}

// vim: ft=cpp
