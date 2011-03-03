#include "hwdep.h"
#include "gps.h"

inline char to_hex(unsigned char val) {
  if (val > 10) {
    return 'A' + (val - 10);
  } else {
    return '0' + val;
  }
}

void gps_write_nmea(const char *sentence) {
  unsigned char cksum = 0;
  for (const char *i = sentence; *i; i++) {
    cksum ^= *i;
  }
  gps_writebyte('$');
  gps_write(sentence);
  gps_writebyte('*');
  gps_writebyte(to_hex(cksum >> 4));
  gps_writebyte(to_hex(cksum & 0x0F));
  gps_writebyte('\x0d');
  gps_writebyte('\x0a');
}

void gps_set_nmea_reporting() {
  gps_write_nmea("PSRF103,00,00,01,01"); // GPGGA 1sec
  gps_write_nmea("PSRF103,01,00,01,01"); // GPGLL off
  gps_write_nmea("PSRF103,02,00,01,01"); // GPGSA 1sec
  gps_write_nmea("PSRF103,03,00,10,01"); // GPGSV 10sec
  gps_write_nmea("PSRF103,04,00,01,01"); // GPRMC 1sec
  gps_write_nmea("PSRF103,05,00,00,01"); // GPVTG off
  gps_write_nmea("PSRF103,07,00,01,01"); // GPZDA 1sec
  gps_write_nmea("PSRF151,01"); // Enable WAAS */
}

void gps_set_sirf() {
  gps_write_nmea("PSRF101,0,0,0,000,0,0,12,8");
  delay(1000);
  gps_write_nmea("PSRF100,0,38400,8,1,0");
  gps_set_baud(38400);
}

static enum gps_state_t decoder_state;
static char gps_ignore_payload;
static char gps_message_valid;
static unsigned int gps_payload_len;
static unsigned int gps_payload_remain;
static char gps_payload[GPS_BUFFER_SIZE];
static unsigned int gps_payload_checksum;
static unsigned int msg_checksum;
static char *gps_payload_ptr;

void gps_handle_message();

void gps_poll() {
  while (gps_can_read()) {
    int ch = gps_read();
    switch (decoder_state) {
      case GPS_HEADER1:
        if (ch == 0xa0)
          decoder_state = GPS_HEADER2;
        else {
          debug("Got garbage char "); debug_int(ch); debug(" from GPS\n");
        }
        break;
      case GPS_HEADER2:
        if (ch == 0xa2)
          decoder_state = GPS_LENGTH1;
        else {
          debug("Got garbage char "); debug_int(ch); debug(" from GPS (expecting 2nd header byte)\n");
          decoder_state = GPS_HEADER1;
        }
        break;
      case GPS_LENGTH1:
        gps_payload_len = (ch & 0x7f) << 8;
        decoder_state = GPS_LENGTH2;
        break;
      case GPS_LENGTH2:
        gps_payload_len += ch;
        gps_payload_remain = gps_payload_len;
        if (gps_payload_len > GPS_BUFFER_SIZE - 1) {
          debug_int(gps_payload_len); debug(" byte payload too big\n");
          gps_ignore_payload = 1;
        } else {
          gps_message_valid = 1;
          gps_ignore_payload = 0;
          gps_payload_checksum = 0;
          gps_payload_ptr = gps_payload;
        }
        decoder_state = GPS_PAYLOAD;
        break;
      case GPS_PAYLOAD:
        if (!gps_ignore_payload) {
          *(gps_payload_ptr++) = ch;
          gps_payload_checksum += ch;
        }
        if (--gps_payload_remain == 0)
          decoder_state = GPS_CHECKSUM1;
        break;
      case GPS_CHECKSUM1:
        msg_checksum = ch << 8;
        decoder_state = GPS_CHECKSUM2;
        break;
      case GPS_CHECKSUM2:
        msg_checksum += ch;
        if (!gps_ignore_payload) {
          gps_payload_checksum &= 0x7FFF;
          if (gps_payload_checksum != msg_checksum) {
            debug("Received checksum "); debug_int(msg_checksum);
            debug(" != calculated "); debug_int(gps_payload_checksum);
            debug(", discarding message\n");
            gps_message_valid = 0;
          }
        }
        decoder_state = GPS_TRAILER1;
        break;
      case GPS_TRAILER1:
        if (ch == 0xb0)
          decoder_state = GPS_TRAILER2;
        else {
          debug("GPS got garbage "); debug_int(ch); debug(" (expecting 1st trailer)\n");
          decoder_state = GPS_HEADER1;
        }
        break;
      case GPS_TRAILER2:
        if (ch == 0xb3) {
          if (gps_message_valid)
            gps_handle_message();
        } else {
          debug("GPS Got garbage "); debug_int(ch); debug(" (expecting 2nd trailer\n");
        }
        decoder_state = GPS_HEADER1;
        break;
      default:
        debug("GPS decoder in unknown state?\n");
        decoder_state = GPS_HEADER1;
    }
  }
}

void gps_handle_message() {
  unsigned char message_type = gps_payload[0];
  debug("Got "); debug_int(gps_payload_len); 
  debug(" byte message, type "); debug_int((int)message_type);
  debug("\n");
}
