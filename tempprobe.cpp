#include "config.h"
int tempprobe_corr = 0;
#ifdef TEMPCORR

#  ifdef SIMULATE

void tempprobe_init() { }
void tempprobe_int() { }
float tempprobe_gettemp() { return 0.0f; }

#  else

#include <OneWire.h>
#include <DallasTemperature.h>
#include "hwdep.h"
#include "tempprobe.h"

OneWire tempprobe_1wire(3);
DallasTemperature tempprobe(&tempprobe_1wire);
unsigned char have_probe = 0;
unsigned char probe_addr[8];
unsigned char count = 0;
unsigned char temp_valid = 0;
float tempprobe_temp = 0.0f;

void tempprobe_init() {
  tempprobe.begin();
  tempprobe.setWaitForConversion(false);
  int probes = tempprobe.getDeviceCount();
  if (probes) {
    have_probe = 1;
    tempprobe.getAddress(probe_addr, 0);
    debug("Found temp probe\n");
  } else {
    have_probe = 0;
    debug("Temp probe not found\n");
  }
  tempprobe.setResolution(probe_addr, 12);
}

/* Called every 32.5ms by mainloop */
void tempprobe_run() {
  if (!have_probe) {
    return;
  }

  if (count == 0) {
    tempprobe.requestTemperaturesByAddress(probe_addr);
    count++;
  } else if (count == 26) {
    tempprobe_temp = tempprobe.getTempC(probe_addr);
    tempprobe_corr = (int)(16.0 * (0.00295255 * tempprobe_temp * tempprobe_temp - 0.226303 * tempprobe_temp + 33.2286) + 0.5);
    temp_valid = 1;
    count++;
  } else if (count == 96) {
    count = 0;
  } else {
    count++;
  }
}

float tempprobe_gettemp() {
  return tempprobe_temp;
}

#  endif
#endif
