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
ARDUINO_LIBS=Ethernet Ethernet/utility SPI
TARGET=ntpserver
MCU=atmega2560
F_CPU=16000000
ARDUINO_PORT=/dev/ttyACM0
AVRDUDE_ARD_PROGRAMMER=stk500v2
AVRDUDE_ARD_BAUDRATE=115200

include $(ARDUINO_DIR)/Arduino.mk

# Work around broken lib support
ETHER_OBJS = build-cli/Ethernet.o build-cli/Server.o build-cli/Client.o build-cli/Udp.o build-cli/w5100.o build-cli/socket.o
SPI_OBJS = build-cli/SPI.o
TEMPPROBE_OBJS = build-cli/OneWire.o build-cli/DallasTemperature.o
EXTLIB_OBJS = $(ETHER_OBJS) $(SPI_OBJS) $(TEMPPROBE_OBJS)

build-cli/%.o: /usr/share/arduino/libraries/Ethernet/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

build-cli/%.o: /usr/share/arduino/libraries/Ethernet/utility/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

build-cli/%.o: /usr/share/arduino/libraries/SPI/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

build-cli/%.o: OneWire/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

build-cli/%.o: DallasTemperature/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(TARGET_ELF): $(OBJS) $(SYS_OBJS) $(EXTLIB_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(SYS_OBJS) $(EXTLIB_OBJS)

hex: build-cli/ntpserver.hex

cleansim:
	rm sim/*.o

cleanarduino:
	rm build-cli/*

clean: cleansim cleanarduino

.PHONY: sim hex cleansim cleanarduino clean
