#include "hwdep.h"
#include "config.h"
#include "ethernet.h"

volatile char ether_int = 0;

#ifdef SIMULATE

void ether_init() {
}

void ether_poll() {
}

#else

#include <Ethernet.h>
#include <EthernetDHCP.h>
#include <Udp.h>
#include "w5100.h"

unsigned char mac[] = { 0x9a, 0xa2, 0xda, 0x00, 0x32, 0xd4 };
unsigned char ip[] = { 192, 168, 1, 201 };

Server debugserver = Server(1000);
Client debugclient = Client(MAX_SOCK_NUM);

static uint32 recv_ts_upper, recv_ts_lower;
extern const int32 NTP_FUDGE_RX, NTP_FUDGE_TX;

void ether_interrupt() {
  if (!ether_int) {
    time_get_ntp(&recv_ts_upper, &recv_ts_lower, NTP_FUDGE_RX);
    ether_int = 1;
  }
}

void clear_ether_interrupt() {
  cli();
  W5100.writeIR(0xF0);
  W5100.writeSnIR(0, 0xff);
  W5100.writeSnIR(1, 0xff);
  W5100.writeSnIR(2, 0xff);
  W5100.writeSnIR(3, 0xff);
  ether_int = 0;
  sei();
}

void ether_init() {
  pinMode(2, INPUT);
  attachInterrupt(0, ether_interrupt, FALLING);

#ifdef DHCP
  debug("Starting DHCP...\n");
  EthernetDHCP.begin(mac);
#else
  Ethernet.begin(mac, ip);
#endif
  debug("IP: ");
  debug_int((unsigned int)ip[0]); debug("."); debug_int((unsigned int)ip[1]); debug(".");
  debug_int((unsigned int)ip[2]); debug("."); debug_int((unsigned int)ip[3]); debug("\n");

  W5100.writeIMR(0x0F);
  Udp.begin(123);
  debugserver.begin();
}

static const char ntp_packet_template[48] = {
  4 /* Mode: Server reply */ | 3<<3 /* Version: NTPv3 */,
  1 /* Stratum */, 9 /* Poll: 512sec */, -21 /* Precision: 0.5 usec */,
  0, 0, 0, 0 /* Root Delay */,
  0, 0, 0, 10 /* Root Dispersion */,
  'G', 'P', 'S', 0 /* Reference ID */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Reference Timestamp */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Origin Timestamp */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Receive Timestamp */,
  0, 0, 0, 0, 0, 0, 0, 0 /* Transmit Timestamp */
};

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
    /* Reftime is the last time we updated the PLL. */
    reply[16] = (reftime_upper >> 24) & 0xff;
    reply[17] = (reftime_upper >> 16) & 0xff;
    reply[18] = (reftime_upper >> 8) & 0xff;
    reply[19] = (reftime_upper) & 0xff;
    reply[20] = (reftime_lower >> 24) & 0xff;
    reply[21] = (reftime_lower >> 16) & 0xff;
    reply[22] = (reftime_lower >> 8) & 0xff;
    reply[23] = (reftime_lower) & 0xff;

    time_get_ntp(&tx_ts_upper, &tx_ts_lower, NTP_FUDGE_TX);
    /* Copy tx timestamp into packet */
    reply[40] = (tx_ts_upper >> 24) & 0xff;
    reply[41] = (tx_ts_upper >> 16) & 0xff;
    reply[42] = (tx_ts_upper >> 8) & 0xff;
    reply[43] = (tx_ts_upper) & 0xff;
    reply[44] = (tx_ts_lower >> 24) & 0xff;
    reply[45] = (tx_ts_lower >> 16) & 0xff;
    reply[46] = (tx_ts_lower >> 8) & 0xff;
    reply[47] = (tx_ts_lower) & 0xff;
    Udp.sendPacket(reply, 48, ip, port);
    debug("NTP\n");
  } else {
    debug("NTP unknown packet type\n");
  }
}

void ether_poll() {
  unsigned char buf[64];
  unsigned char clientip[4];
  unsigned int port;
  unsigned int len;

  do {
    while (Udp.available()) {
      len = Udp.readPacket(buf, 256, clientip, &port);
      do_ntp_request(buf, len, clientip, port);
    }
    if (debugclient.connected()) {
      while (debugclient.available()) {
        debugclient.read();
        debugserver.print("Hi!\n");
      }
    } else {
      debugclient = debugserver.available();
    }
    clear_ether_interrupt();
  } while (ether_int);
}

static unsigned int dhcp_timer = 0;
void ether_dhcp_poll() {
  dhcp_timer++;
  /* If we're waiting for something from the server, then poll every int.
   * Otherwise, poll once per minute plus one int (just so it doesn't always
   * land at the same point in the second :)
   */
  if (EthernetDHCP._state != DhcpStateLeased
      || dhcp_timer >= 32 * 60L + 1) {
    dhcp_timer = 0;
    EthernetDHCP.poll();
  }
}


#endif
