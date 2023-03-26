CXX=g++
# -std=gnu++17
CXXFLAGS=-std=gnu++14 -DOSPI -Wall
LD=$(CXX)
LIBS=pthread mosquitto
LDFLAGS=$(addprefix -l,$(LIBS))
BINARY=OpenSprinkler
SOURCES=main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp
HEADERS=$(wildcard *.h)
OBJECTS=$(SOURCES:.cpp=.o)

.PHONY: all
all: $(BINARY)

%.o: %.cpp $(HEADERS)
	$(CXX) -c -o "$@" $(CXXFLAGS) "$<"

$(BINARY): $(OBJECTS)
	$(CXX) -o $(BINARY) $(OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(BINARY)
