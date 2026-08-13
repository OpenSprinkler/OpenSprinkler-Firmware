CXX=g++
# -std=gnu++17
VERSION?=OSPI
CXXFLAGS=-std=gnu++14 -D$(VERSION) -DSMTP_OPENSSL -Wall -I. -include string.h -include cstdint -Iexternal/TinyWebsockets/tiny_websockets_lib/include -Iexternal/OpenThings-Framework-Firmware-Library/ $(EXTRA_CXXFLAGS)
LD=$(CXX)
LIBS=pthread mosquitto ssl crypto
ifeq ($(VERSION),OSPI)
LIBS+=i2c lgpio
endif
LDFLAGS=$(addprefix -l,$(LIBS))
BINARY=OpenSprinkler
SERVICE_SOURCES=$(filter-out services/EMailSender.cpp services/espconnect.cpp,$(wildcard services/*.cpp)) $(wildcard services/*.c)
SOURCES=main.cpp OpenSprinkler.cpp $(wildcard api/*.cpp) $(wildcard boards/*.cpp) $(wildcard core/*.cpp) $(wildcard drivers/*.cpp) $(wildcard platform/*.cpp) $(wildcard sensors/*.cpp) $(SERVICE_SOURCES) $(wildcard storage/*.cpp) $(wildcard util/*.cpp) $(wildcard external/TinyWebsockets/tiny_websockets_lib/src/*.cpp) $(wildcard external/OpenThings-Framework-Firmware-Library/*.cpp)
HEADERS=$(wildcard *.h) $(wildcard *.hpp) $(wildcard api/*.h) $(wildcard boards/*.h) $(wildcard core/*.h) $(wildcard drivers/*.h) $(wildcard platform/*.h) $(wildcard sensors/*.h) $(wildcard services/*.h) $(wildcard storage/*.h) $(wildcard util/*.h) external/ArduinoJson.hpp
OBJECTS=$(addsuffix .o,$(basename $(SOURCES)))

.PHONY: all
all: $(BINARY)

%.o: %.cpp %.c $(HEADERS)
	$(CXX) -c -o "$@" $(CXXFLAGS) "$<"

$(BINARY): $(OBJECTS)
	$(CXX) -o $(BINARY) $(OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) gpio.o i2cd.o RCSwitch.o ads1115.o program.o notifier.o weather.o mqtt.o smtp.o EMailSender.o espconnect.o utils.o $(BINARY)

.PHONY: container
container:
	docker build .

TEST_HTTP_PORT?=18080

.PHONY: test-api
test-api: test-board-profiles test-hardware-detection test-storage-files test-firmware-release test-buffer-filler test-string-buffer test-sensor-units test-weather-sensor-cache
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

.PHONY: test-firmware-release
test-firmware-release:
	python3 -B -m unittest tests/test_firmware_release.py

.PHONY: test-sensor-units
test-sensor-units:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections \
			tests/sensor_unit_test.cpp sensors/sensor.cpp sensors/weather_sensor.cpp \
			-Wl,--gc-sections -o "$$output"; \
		"$$output"

.PHONY: test-buffer-filler
test-buffer-filler:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. tests/bfiller_test.cpp -o "$$output"; \
		"$$output"

.PHONY: test-string-buffer
test-string-buffer:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections \
			tests/string_buffer_test.cpp util/utils.cpp api/http.cpp \
			-Wl,--gc-sections -o "$$output"; \
		"$$output"

.PHONY: test-weather-sensor-cache
test-weather-sensor-cache:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections \
			tests/weather_sensor_cache_test.cpp services/weather.cpp \
			-Wl,--gc-sections -o "$$output"; \
		"$$output"
