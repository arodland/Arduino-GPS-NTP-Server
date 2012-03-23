#ifndef __LCD_H
#define __LCD_H

#ifdef SIMULATE
#define lcd_write(x, ...) do { } while (0)
#else
#define LCDPORT Serial3
#define lcd_write(x, ...) LCDPORT.print(x, ##__VA_ARGS__)
#endif

void lcd_init();

extern void lcd_set_displaydate(unsigned int year, unsigned int month, unsigned int day,
    unsigned int hour, unsigned int minute, unsigned int second, int utc_offset);

extern void lcd_set_gps_status(unsigned int numsvs);

extern void lcd_set_pll_status(int32 offset, short freq);

void lcd_draw();

#endif
