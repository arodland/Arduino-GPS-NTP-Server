SIMCXX=g++
SIMCXXFLAGS=-DF_CPU=16000000 -DSIMULATE -g

SIMOBJS = $(patsubst %.cpp,sim/%.o,$(wildcard *.cpp))

$(SIMOBJS) : *.h

timesim: $(SIMOBJS)
	$(SIMCXX) $(SIMCXXFLAGS) -o timesim $(SIMOBJS)

sim/%.o : %.cpp
	$(SIMCXX) $(SIMCXXFLAGS) -c -o $@ $<

sim: timesim

ARDUINO_DIR=/usr/share/arduino
TARGET=ntpserver
MCU=atmega2560
F_CPU=16000000
ARDUINO_PORT=/dev/ttyACM0
AVRDUDE_ARD_PROGRAMMER=stk500v2
AVRDUDE_ARD_BAUDRATE=115200

include $(ARDUINO_DIR)/Arduino.mk

hex: build-cli/ntpserver.hex

.PHONY: sim hex
