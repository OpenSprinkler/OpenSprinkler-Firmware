CXX=g++
# -std=gnu++17
VERSION=OSPI
CXXFLAGS=-std=gnu++14 -D$(VERSION) -DSMTP_OPENSSL -Wall -include string.h -Iexternal/TinyWebsockets/tiny_websockets_lib/include -Iexternal/OpenThings-Framework-Firmware-Library/
LD=$(CXX)
LIBS=pthread mosquitto ssl crypto
LDFLAGS=$(addprefix -l,$(LIBS))
BINARY=OpenSprinkler
SOURCES=$(wildcard src/*.cpp) $(wildcard src/*.c) $(wildcard external/TinyWebsockets/tiny_websockets_lib/src/*.cpp) $(wildcard external/OpenThings-Framework-Firmware-Library/*.cpp)
HEADERS=$(wildcard include/*.h) $(wildcard include/*.hpp)
OBJECTS=$(addsuffix .o,$(basename $(SOURCES)))

.PHONY: all
all: $(BINARY)

%.o: %.cpp %.c $(HEADERS)
	$(CXX) -c -o "$@" $(CXXFLAGS) "$<"

$(BINARY): $(OBJECTS)
	$(CXX) -o $(BINARY) $(OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(BINARY)

.PHONY: container
container:
	docker build .
