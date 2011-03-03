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

void gps_write_sirf(const char *sentence, int len) {
  unsigned short cksum = 0;
  gps_write("\xa0\xa2");
  gps_writebyte(len >> 8);
  gps_writebyte(len & 0xff);
  for (int i = 0 ; i < len ; i++) {
    gps_writebyte(sentence[i]);
    cksum += (unsigned char) sentence[i];
  }
  gps_writebyte((cksum & 0x7fff) >> 8);
  gps_writebyte(cksum & 0xff);
  gps_write("\xb0\xb3");
}

void gps_enable_dgps() {
  gps_write_sirf("\x84\x00", 2);
  /* Cmd: 128, 22 bytes unused, 20 channels, enable navlib */
  // gps_write_sirf("\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14\x10", 25);
  /* Cmd: 128, 22 bytes unused, 20 channels, disable navlib */
//  gps_write_sirf("\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14\x00", 25);

  /* Cmd: 133, DGPS source: 1, 5 bytes unused */
  gps_write_sirf("\x85\x01\x00\x00\x00\x00\x00", 7);
  gps_write_sirf("\x85\x01\x00\x00\x00\x00\x00", 7);
  /* Cmd: 138, DGPS selection: auto, DGPS timeout: auto */
  gps_write_sirf("\x8a\x00\x00", 3);
  /* Cmd: 170, PRN: auto, mode: testing, timeout: auto, 2 bytes unused */
  gps_write_sirf("\xaa\x00\x00\x00\x00\x00", 6);

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
//  gps_write_nmea("PSRF101,0,0,0,000,0,0,12,8");
  delay(1000);
  gps_write_nmea("PSRF100,0,38400,8,1,0");
  gps_set_baud(38400);
}

static enum gps_state_t decoder_state;
static char gps_ignore_payload;
static char gps_message_valid;
static unsigned int gps_payload_len;
static unsigned int gps_payload_remain;
static unsigned char gps_payload[GPS_BUFFER_SIZE];
static unsigned int gps_payload_checksum;
static unsigned int msg_checksum;
static unsigned char *gps_payload_ptr;

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

void gps_navdata_message() {
}

void gps_tracking_data_message() {
}

void gps_clockstatus_message() {
}

void gps_satvisible_message() {
  unsigned short num_visible = gps_payload[1];
  debug("Visible SVs:");
  for (int i = 0 ; i < num_visible; i++) {
    debug(" ");
    debug_int((int)(gps_payload[2 + 5 * i]));
  }
  debug("\n");
}

void gps_dgps_message() {
  unsigned int dgps_source = gps_payload[1];

  debug("DGPS source: "); debug_int(dgps_source); debug(", PRN:");
  for (int i = 16; i < 52 ; i += 3) {
    debug(" ");
    debug_int((int)(gps_payload[i]));
  }
  debug("\n");
}

void gps_geodetic_message() {
  unsigned int year = gps_payload[11] << 8 | gps_payload[12];
  unsigned int month = gps_payload[13];
  unsigned int day = gps_payload[14];
  unsigned int hour = gps_payload[15];
  unsigned int minute = gps_payload[16];
  unsigned int second = gps_payload[17] << 8 | gps_payload[18];

  unsigned int numsvs = gps_payload[88];

  debug_int(year); debug("-"); debug_int(month); debug("-"); debug_int(day);
  debug(" ");
  debug_int(hour); debug(":"); debug_int(minute); debug(":");
  debug_int(second/1000); debug("."); debug_int(second % 1000);
  debug(", ");
  debug_int(numsvs);
  debug(" SVs\n");
}

void gps_ack_message() {
  debug("Got ACK for message ");
  debug_int((int)gps_payload[1]);
  debug("\n");
}

void gps_nak_message() {
  debug("Got NAK for message ");
  debug_int((int)gps_payload[1]);
  debug("\n");
}
void gps_handle_message() {
  unsigned char message_type = gps_payload[0];
  switch(message_type) {
    case 2:
      gps_navdata_message();
      break;
    case 4:
      gps_tracking_data_message();
      break;
    case 7:
      gps_clockstatus_message();
      break;
    case 11:
      gps_ack_message();
      break;
    case 12:
      gps_nak_message();
      break;
    case 13:
      gps_satvisible_message();
      break;
    case 27:
      gps_dgps_message();
      break;
    case 41:
      gps_geodetic_message();
      break;
    case 28:
      debug("NavLib msg for sat ");
      debug_int((int)gps_payload[6]);
      debug(" sync flags ");
      debug_int((int)gps_payload[37]);
      debug("\n");
      break;
    case 29:
      debug("NavLib DGPS msg for sat ");
      debug_int((int)(gps_payload[1] << 8 + gps_payload[0]));
      debug("\n");
      break;

    case 9:
      break;
    default:
      debug("Got "); debug_int(gps_payload_len);
      debug(" byte message, type "); debug_int((int)message_type);
      debug("\n");
      break;
  }
}
