CXX=g++
# -std=gnu++17
VERSION?=OSPI
CXXFLAGS=-std=gnu++14 -D$(VERSION) -DSMTP_OPENSSL -Wall -include string.h -include cstdint -Iexternal/TinyWebsockets/tiny_websockets_lib/include -Iexternal/OpenThings-Framework-Firmware-Library/
LD=$(CXX)
LIBS=pthread mosquitto ssl crypto i2c lgpio
LDFLAGS=$(addprefix -l,$(LIBS))
BINARY=OpenSprinkler
SOURCES=main.cpp OpenSprinkler.cpp notifier.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp mqtt.cpp smtp.c RCSwitch.cpp i2cd.cpp ads1115.cpp $(wildcard sensors/*.cpp) $(wildcard external/TinyWebsockets/tiny_websockets_lib/src/*.cpp) $(wildcard external/OpenThings-Framework-Firmware-Library/*.cpp)
HEADERS=$(wildcard *.h) $(wildcard *.hpp) $(wildcard sensors/*.h)
OBJECTS=$(addsuffix .o,$(basename $(SOURCES)))

.PHONY: all
all: $(BINARY)

# Makefile is a prerequisite so that changing CXXFLAGS (or these rules) forces
# a rebuild. Without it, an object built by an older rule is considered up to
# date and silently survives -- which is exactly how a pre-existing smtp.o
# would keep the flagless build described below.
%.o: %.cpp $(HEADERS) Makefile
	$(CXX) -c -o "$@" $(CXXFLAGS) "$<"

# smtp.c is the only C source. It must be built with the same flags as the C++
# sources -- exactly as build.sh does by passing it to g++ on one command line.
# The previous rule was "%.o: %.cpp %.c", which requires BOTH a .cpp and a
# same-stem .c to exist; no such pair does, so the rule never matched and
# smtp.o fell through to make's built-in "cc -c" with no flags at all. That
# silently dropped -DSMTP_OPENSSL, compiling out every TLS/STARTTLS branch in
# smtp.c and sending SMTP credentials in cleartext.
%.o: %.c $(HEADERS) Makefile
	$(CXX) -c -o "$@" $(CXXFLAGS) "$<"

$(BINARY): $(OBJECTS)
	$(CXX) -o $(BINARY) $(OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(BINARY)

.PHONY: container
container:
	docker build .
