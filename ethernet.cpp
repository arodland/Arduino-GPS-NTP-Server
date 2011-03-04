#include "hwdep.h"
#include "ethernet.h"

#ifdef SIMULATE

void ether_init() {
}

void ether_poll() {
}

#else

#include <Ethernet.h>
#include <Udp.h>

unsigned char mac[] = { 0x00, 0x15, 0xc5, 0x5a, 0xd1, 0x06 };
unsigned char ip[] = { 192, 168, 6, 115 };

Server debugserver = Server(1000);
Client debugclient = Client(MAX_SOCK_NUM);

void ether_init() {
  Ethernet.begin(mac, ip);

  unsigned char destip[] = { 224, 0, 0, 2 };
  unsigned char msg[] = "Hi";
  Udp.begin(123);
  Udp.sendPacket(msg, 2, destip, 123);

  debugserver.begin();
}

static const char ntp_packet_template[48] = {
  4 /* Mode: Server reply */ | 3<<3 /* Version: NTPv3 */,
  1 /* Stratum */, 9 /* Poll: 512sec */, -19 /* Precision: 0.5 usec */,
  0, 0, 0, 0 /* Root Delay */,
  0, 0, 0, 10 /* Root Dispersion */,
  'G', 'P', 'S', 0 /* Reference ID */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Reference Timestamp */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Origin Timestamp */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Receive Timestamp */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Transmit Timestamp */
};

static uint32 recv_ts_upper, recv_ts_lower;

void do_ntp_request(unsigned char *buf, unsigned int len,
    unsigned char *ip, unsigned int port) {
  unsigned char version = (buf[0] >> 3) & 7;
  unsigned char mode = buf[0] & 7;

  if (len < 48) {
    debug("Not NTP\n");
    return;
  }

  if (version != 3 && version != 4) {
    debug("NTP Unknown Version\n");
    return;
  }
  
  if (mode == 3) { /* Client request */
    unsigned char reply[48];
    uint32 tx_ts_upper, tx_ts_lower;
    memcpy(reply, ntp_packet_template, 48);
    /* XXX set Leap Indicator */
    /* Copy client transmit timestamp into origin timestamp */
    memcpy(reply + 24, buf + 40, 8);
    /* Copy receive timestamp into packet */
    reply[32] = (recv_ts_upper >> 24) & 0xff;
    reply[33] = (recv_ts_upper >> 16) & 0xff;
    reply[34] = (recv_ts_upper >> 8) & 0xff;
    reply[35] = (recv_ts_upper) & 0xff;
    reply[36] = (recv_ts_lower >> 24) & 0xff;
    reply[37] = (recv_ts_lower >> 16) & 0xff;
    reply[38] = (recv_ts_lower >> 8) & 0xff;
    reply[39] = (recv_ts_lower) & 0xff;
    /* Copy top half of receive timestamp into reference timestamp --
     * we update clock every second :)
     */
    memcpy(reply + 16, reply + 32, 4);

    time_get_ntp(&tx_ts_upper, &tx_ts_lower);
    /* Copy tx timestamp into packet */
    reply[40] = (tx_ts_upper >> 24) & 0xff;
    reply[41] = (tx_ts_upper >> 16) & 0xff;
    reply[42] = (tx_ts_upper >> 8) & 0xff;
    reply[43] = (tx_ts_upper) & 0xff;
    reply[44] = (tx_ts_lower >> 24) & 0xff;
    reply[45] = (tx_ts_lower >> 16) & 0xff;
    reply[46] = (tx_ts_lower >> 8) & 0xff;
    reply[47] = (tx_ts_lower) & 0xff;
    debug("NTP\n");
    Udp.sendPacket(reply, 48, ip, port);
  } else {
    debug("NTP unknown packet type");
  }
}

void ether_poll() {
  unsigned char buf[64];
  unsigned char clientip[4];
  unsigned int port;
  unsigned int len;


  if (Udp.available()) {
    time_get_ntp(&recv_ts_upper, &recv_ts_lower);
    len = Udp.readPacket(buf, 256, clientip, &port);
    do_ntp_request(buf, len, clientip, port);
  }
  if (debugclient.connected()) {
    if (debugclient.available()) {
      debugclient.read();
      debugserver.print("Hi!\n");
    }
  } else {
    debugclient = debugserver.available();
  }
}

#endif
