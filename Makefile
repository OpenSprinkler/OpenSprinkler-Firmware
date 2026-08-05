CXX=g++
# -std=gnu++17
VERSION?=OSPI
CXXFLAGS=-std=gnu++14 -D$(VERSION) -DSMTP_OPENSSL -Wall -include string.h -include cstdint -Iexternal/TinyWebsockets/tiny_websockets_lib/include -Iexternal/OpenThings-Framework-Firmware-Library/ $(EXTRA_CXXFLAGS)
LD=$(CXX)
LIBS=pthread mosquitto ssl crypto
ifeq ($(VERSION),OSPI)
LIBS+=i2c lgpio
endif
LDFLAGS=$(addprefix -l,$(LIBS))
BINARY=OpenSprinkler
SOURCES=main.cpp OpenSprinkler.cpp notifier.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp mqtt.cpp smtp.c $(wildcard boards/*.cpp) $(wildcard drivers/*.cpp) $(wildcard platform/*.cpp) $(wildcard sensors/*.cpp) $(wildcard storage/*.cpp) $(wildcard external/TinyWebsockets/tiny_websockets_lib/src/*.cpp) $(wildcard external/OpenThings-Framework-Firmware-Library/*.cpp)
HEADERS=$(wildcard *.h) $(wildcard *.hpp) $(wildcard boards/*.h) $(wildcard drivers/*.h) $(wildcard platform/*.h) $(wildcard sensors/*.h) $(wildcard storage/*.h)
OBJECTS=$(addsuffix .o,$(basename $(SOURCES)))

.PHONY: all
all: $(BINARY)

%.o: %.cpp %.c $(HEADERS)
	$(CXX) -c -o "$@" $(CXXFLAGS) "$<"

$(BINARY): $(OBJECTS)
	$(CXX) -o $(BINARY) $(OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) gpio.o i2cd.o RCSwitch.o ads1115.o $(BINARY)

.PHONY: container
container:
	docker build .

TEST_HTTP_PORT?=18080

.PHONY: test-api
test-api: test-board-profiles test-hardware-detection test-storage-files
	$(MAKE) clean
	$(MAKE) VERSION=DEMO EXTRA_CXXFLAGS="-DHTTP_PORT=$(TEST_HTTP_PORT)"
	python3 tests/api_contract.py --port $(TEST_HTTP_PORT)

.PHONY: test-board-profiles
test-board-profiles:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/board_profile_test.cpp boards/board_profile.cpp -o "$$output"; \
		"$$output"

.PHONY: test-hardware-detection
test-hardware-detection:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/hardware_detection_test.cpp boards/hardware_detection.cpp -o "$$output"; \
		"$$output"

.PHONY: test-storage-files
test-storage-files:
	@set -e; output=$$(mktemp); data=$$(mktemp -d); trap 'rm -f "$$output"; rm -rf "$$data"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. tests/storage_files_test.cpp storage/files.cpp -o "$$output"; \
		"$$output" "$$data"
