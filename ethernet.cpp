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

  attachInterrupt(2, ether_poll, CHANGE);
}

void ether_poll() {
  if (debugclient.connected()) {
    if (debugclient.available()) {
      debug("Client read\n");
      debugclient.read();
      debugserver.print("Hi!\n");
    }
  } else {
    debugclient = debugserver.available();
    if (debugclient) {
      debug("Client connect");
    }
  }
}

#endif
