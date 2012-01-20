SIMCXX=g++
SIMCXXFLAGS=-DF_CPU=16000000 -DSIMULATE -g

SIMOBJS = $(patsubst %.cpp,sim/%.o,$(wildcard *.cpp))

all: sim hex


$(SIMOBJS) : *.h

timesim: $(SIMOBJS)
	$(SIMCXX) $(SIMCXXFLAGS) -o timesim $(SIMOBJS)

sim/%.o : %.cpp
	$(SIMCXX) $(SIMCXXFLAGS) -c -o $@ $<

sim: timesim

ARDUINO_DIR=/usr/share/arduino
ARDUINO_LIB_PATH=$(ARDUINO_DIR)/libraries .
ARDUINO_LIBS=Ethernet/utility Ethernet ArduinoEthernet/EthernetDHCP/utility ArduinoEthernet/EthernetDHCP OneWire SPI DallasTemperature
AVR_TOOLS_PATH=/usr/bin
TARGET=ntpserver
MCU=atmega2560
F_CPU=16000000
ARDUINO_PORT=/dev/ttyACM0
AVRDUDE_ARD_PROGRAMMER=stk500v2
AVRDUDE_ARD_BAUDRATE=115200
AVRDUDE_CONF=/usr/share/arduino/hardware/tools/avrdude.conf

include Arduino.mk

hex: build-cli/ntpserver.hex

cleansim:
	rm sim/*.o

cleanarduino:
	rm -r build-cli/libs
	rm build-cli/*

clean: cleansim cleanarduino

.PHONY: sim hex cleansim cleanarduino clean
