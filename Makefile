ARDUINO=/usr/share/arduino
CXX=g++
AVRCXX=avr-g++
CXXFLAGS=-DF_CPU=16000000
CXXFLAGS_SIMULATE=-DSIMULATE
CXXFLAGS_ARDUINO=-mmcu=atmega2560 -ffunction-sections -fdata-sections -I$(ARDUINO)/hardware/arduino/cores/arduino

all: timesim timesim.arduino

timesim: sim/simmain.o sim/hwdep.o sim/timing.o
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_SIMULATE) -o timesim sim/simmain.o sim/hwdep.o sim/timing.o

timesim.arduino: avr/main.o avr/hwdep.o avr/timing.o
	$(AVRCXX) $(CXXFLAGS) $(CXXFLAGS_ARDUINO) -o timesim.arduino avr/main.o avr/hwdep.o avr/timing.o

sim/%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_SIMULATE) -c -o $@ $<

avr/%.o : %.cpp
	$(AVRCXX) $(CXXFLAGS) $(CXXFLAGS_ARDUINO) -c -o $@ $<
