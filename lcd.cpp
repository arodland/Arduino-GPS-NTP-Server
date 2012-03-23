#include "hwdep.h"
#include "lcd.h"
#include <string.h>

#ifndef SIMULATE

void lcd_hide_cursor() {
  lcd_write(4, BYTE);
}

void lcd_set_backlight(unsigned char brightness) {
  lcd_write(14, BYTE);
  lcd_write(brightness, BYTE);
}

void lcd_set_contrast(unsigned char contrast) {
  lcd_write(15, BYTE);
  lcd_write(contrast, BYTE);
}

void lcd_home() {
  lcd_write(12, BYTE);
}

void lcd_cursor(unsigned char row, unsigned char column) {
  lcd_write(17, BYTE);
  lcd_write(column, BYTE);
  lcd_write(row, BYTE);
}

#endif

static char display_date[] = "0000-00-00";
static char display_time[] = "00:00:00";
static char display_utc_offset[] = "+ 0";

void lcd_set_displaydate(unsigned int year, unsigned int month, unsigned int day,
    unsigned int hour, unsigned int minute, unsigned int second, int utc_offset) {

  display_date[0] = '0' + (year / 1000);
  display_date[1] = '0' + ((year % 1000) / 100);
  display_date[2] = '0' + ((year % 100) / 10);
  display_date[3] = '0' + (year % 10);

  display_date[5] = '0' + (month / 10);
  display_date[6] = '0' + (month % 10);

  display_date[8] = '0' + (day / 10);
  display_date[9] = '0' + (day % 10);

  display_time[0] = '0' + (hour / 10);
  display_time[1] = '0' + (hour % 10);
  
  display_time[3] = '0' + (minute / 10);
  display_time[4] = '0' + (minute % 10);

  display_time[6] = '0' + (second / 10);
  display_time[7] = '0' + (second % 10);

  if (utc_offset >= 0) {
    display_utc_offset[0] = '+';
    display_utc_offset[1] = '0' + (utc_offset / 10);
    display_utc_offset[2] = '0' + (utc_offset % 10);
  } else {
    display_utc_offset[0] = '-';
    display_utc_offset[1] = '0' + ((0 - utc_offset) / 10);
    display_utc_offset[2] = '0' + ((0 - utc_offset) % 10);
  }
}

static char gps_status[] = "       ";
static int gps_numsvs = 0;

void lcd_set_gps_status(unsigned int numsvs) {
  gps_numsvs = numsvs;
  if (numsvs >= 3) {
    strcpy(gps_status, "GPS OK");
  } else {
    strcpy(gps_status, "NO LOCK");
  }
}

static char display_offset[] = "+000000000";
static char display_freq[] = "+00000";

void lcd_set_pll_status(int32 offset, short freq) {
  if (offset >= 0) {
    display_offset[0] = '+';
  } else {
    display_offset[0] = '-';
    offset = 0 - offset;
  }

  for (int i = 9; i >= 1 ; i--) {
    if (offset) {
      display_offset[i] = '0' + (offset % 10);
    } else {
      display_offset[i] = ' ';
    }
    offset /= 10;
  }

  if (freq >= 0) {
    display_freq[0] = '+';
  } else {
    display_freq[0] = '-';
    freq = 0 - freq;
  }

  for (int i = 5 ; i >= 1 ; i--) {
    if (freq) {
      display_freq[i] = '0' + (freq % 10);
    } else {
      display_freq[i] = ' ';
    }
    freq /= 10;
  }
}

#ifndef SIMULATE

void lcd_init() {
  LCDPORT.begin(19200);
  lcd_hide_cursor();
  lcd_set_backlight(10);
  lcd_set_contrast(70);
  lcd_home();
  lcd_cursor(0, 13); lcd_write("UTC");
  lcd_cursor(1, 16); lcd_write("sec");
  lcd_cursor(2, 10); lcd_write("ns");
  lcd_cursor(3, 0); lcd_write("PLL");
  lcd_cursor(3, 15); lcd_write("Sats");
}

void lcd_draw() {
  lcd_cursor(0, 0);
  lcd_write(display_date);
  lcd_cursor(1, 0);
  lcd_write(display_time);
  lcd_cursor(1, 13);
  lcd_write(display_utc_offset);

  lcd_cursor(2, 0);
  lcd_write(display_offset);
  lcd_cursor(3, 4);
  lcd_write(display_freq);

  lcd_cursor(2, 13);
  lcd_write(gps_status);
  lcd_cursor(3, 13);
  lcd_write(gps_numsvs);
}

#else

void lcd_init() { }

void lcd_draw() {
#if 0
  debug(display_date); debug("\n");
  debug(display_time); debug("\n");
  debug(display_utc_offset); debug("\n");

  debug(display_offset); debug("\n");
  debug(display_freq); debug("\n");
  
  debug(gps_status); debug("\n");
  debug_int(gps_numsvs); debug(" Sats\n");
#endif
}

#endif
