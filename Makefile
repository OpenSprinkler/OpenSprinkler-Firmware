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
HEADERS+=external/ch390/include/ch390_health.h

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

.PHONY: test-updater-notes
test-updater-notes:
	node tests/updater_notes_test.cjs

.PHONY: test-api
test-api: test-hostname test-board-profiles test-hardware-detection test-storage-files test-sprinkler-log test-firmware-release test-buffer-filler test-string-buffer test-sensor-units test-system-internal-sensor test-weather-sensor-cache test-weather-failsafe test-output-sequencer test-bundle-codec test-w5500-frame test-ch390 test-ethernet-lifecycle test-flow-rate-window test-cycle-soak test-cycle-scheduler test-runtime-queue test-station-extra
	python3 -B -m unittest tests/test_esp8266_stack_guard.py
	$(MAKE) clean
	$(MAKE) VERSION=DEMO EXTRA_CXXFLAGS="-DHTTP_PORT=$(TEST_HTTP_PORT) -DWEATHER_RESPONSE_TEST_MAX_AGE_MS=2000"
	python3 tests/api_contract.py --port $(TEST_HTTP_PORT)

.PHONY: test-cycle-soak test-runtime-queue test-station-extra
.PHONY: test-cycle-scheduler
APP_SOURCE ?= ../OpenSprinkler-App
.PHONY: test-cycle-preview
test-cycle-preview:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -Wall -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections tests/cycle_scheduler_test.cpp \
			core/scheduler.cpp core/program.cpp core/cycle_soak.cpp \
			-Wl,--gc-sections -o "$$output"; \
		"$$output" --preview-fixtures | node tests/cycle_preview_compare.cjs "$(APP_SOURCE)"

test-cycle-scheduler:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -Wall -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections tests/cycle_scheduler_test.cpp \
			core/scheduler.cpp core/program.cpp core/cycle_soak.cpp \
			-Wl,--gc-sections -o "$$output"; "$$output"

test-cycle-soak:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -Wall -Wextra -I. tests/cycle_soak_test.cpp core/cycle_soak.cpp -o "$$output"; \
		"$$output"

test-runtime-queue:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		for platform in ESP8266 ESP32 DEMO; do \
			case "$$platform" in ESP8266) capacity=72;; ESP32) capacity=120;; DEMO) capacity=255;; esac; \
			$(CXX) -std=gnu++14 -Wall -Wextra -I. -D$$platform \
				-DEXPECTED_QUEUE_CAPACITY=$$capacity tests/queue_capacity_test.cpp -o "$$output"; \
			"$$output"; \
		done; \
		$(CXX) -std=gnu++14 -DDEMO -Wall -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections tests/runtime_queue_test.cpp core/program.cpp \
			-Wl,--gc-sections -o "$$output"; \
		"$$output"

test-station-extra:
	@set -e; output=$$(mktemp); data=$$(mktemp -d); trap 'rm -f "$$output"; rm -rf "$$data"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -Wall -Wextra -I. tests/station_extra_test.cpp \
			storage/station_extra.cpp core/cycle_soak.cpp storage/files.cpp \
			storage/maintenance.cpp storage/sprinkler_log.cpp -o "$$output"; \
		"$$output" "$$data"

.PHONY: test-board-profiles
test-board-profiles:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/board_profile_test.cpp boards/board_profile.cpp -o "$$output"; \
		"$$output"

.PHONY: test-flow-rate-window
test-flow-rate-window:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/flow_rate_window_test.cpp -o "$$output"; \
		"$$output"

.PHONY: test-hardware-detection
test-hardware-detection:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/hardware_detection_test.cpp boards/hardware_detection.cpp -o "$$output"; \
		"$$output"

.PHONY: test-storage-files
.PHONY: test-hostname
test-hostname:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. tests/hostname_test.cpp platform/hostname.cpp -o "$$output"; \
		"$$output"; \
		for chip in ESP8266 ESP32; do \
			$(CXX) -std=gnu++14 -DARDUINO -D$$chip -Itests/hostname_stubs -I. \
				tests/hostname_boot_test.cpp platform/hostname.cpp -o "$$output"; \
			"$$output" '' os-01a2ff; \
			"$$output" OS-TEST os-test; \
			"$$output" invalid_name os-01a2ff; \
			"$$output" aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa; \
			"$$output" aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa os-01a2ff; \
		done

test-storage-files:
	@set -e; output=$$(mktemp); data=$$(mktemp -d); trap 'rm -f "$$output"; rm -rf "$$data"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. tests/storage_files_test.cpp \
			storage/files.cpp storage/maintenance.cpp storage/sprinkler_log.cpp platform/hostname.cpp -o "$$output"; \
		"$$output" "$$data"

.PHONY: test-sprinkler-log
test-sprinkler-log:
	@set -e; output=$$(mktemp); data=$$(mktemp -d); trap 'rm -f "$$output"; rm -rf "$$data"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -DSPRINKLER_LOG_TEST_SMALL_GEOMETRY -I. \
			tests/sprinkler_log_test.cpp \
			storage/sprinkler_log.cpp storage/files.cpp storage/maintenance.cpp -o "$$output"; \
		"$$output" "$$data"

.PHONY: test-firmware-release
test-firmware-release:
	python3 -B -m unittest tests/test_firmware_release.py

.PHONY: test-sensor-units
test-api: test-onboard-analog

.PHONY: test-onboard-analog
test-onboard-analog:
	@set -e; output=$$(mktemp); data=$$(mktemp -d); trap 'rm -f "$$output"; rm -rf "$$data"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections \
			tests/onboard_analog_test.cpp sensors/analog_sensor.cpp sensors/ads1115_sensor.cpp \
			sensors/onboard_analog_sensor.cpp sensors/onboard_digital_sensor.cpp sensors/sensor.cpp \
			services/sensor_ports.cpp boards/board_profile.cpp storage/files.cpp \
			-Wl,--gc-sections -o "$$output"; "$$output" "$$data"

test-sensor-units:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections \
			tests/sensor_unit_test.cpp sensors/sensor.cpp sensors/weather_sensor.cpp \
			-Wl,--gc-sections -o "$$output"; \
		"$$output"

.PHONY: test-system-internal-sensor
test-system-internal-sensor:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			-ffunction-sections -fdata-sections \
			tests/system_internal_sensor_test.cpp sensors/system_internal_sensor.cpp sensors/sensor.cpp storage/files.cpp \
			-Wl,--gc-sections -o "$$output"; "$$output"; \
		$(CXX) -std=gnu++14 -DESP32 -Itests/chip_temperature_stubs -I. \
			tests/chip_temperature_test.cpp platform/chip_temperature.cpp -o "$$output"; "$$output"

.PHONY: test-buffer-filler
test-buffer-filler:
	@set -e; output=$$(mktemp); data=$$(mktemp -d); trap 'rm -f "$$output"; rm -rf "$$data"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -I. tests/bfiller_test.cpp storage/files.cpp -o "$$output"; \
		"$$output" "$$data"

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
			tests/weather_sensor_cache_test.cpp services/weather.cpp services/weather_failsafe.cpp \
			platform/monotonic_clock.cpp \
			-Wl,--gc-sections -o "$$output"; \
		"$$output"

.PHONY: test-weather-failsafe
test-weather-failsafe:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/weather_failsafe_test.cpp \
			services/weather_failsafe.cpp -o "$$output"; \
		"$$output"

.PHONY: test-output-sequencer
test-output-sequencer:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/output_sequencer_test.cpp core/output_sequencer.cpp -o "$$output"; \
		"$$output"

.PHONY: test-bundle-codec
test-bundle-codec:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -DDEMO -ffunction-sections -fdata-sections -I. \
			-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
			-Iexternal/OpenThings-Framework-Firmware-Library \
			tests/bundle_codec_test.cpp core/bundle.cpp -Wl,--gc-sections -o "$$output"; \
		"$$output"

.PHONY: test-w5500-frame
test-w5500-frame:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -I. tests/w5500_frame_test.cpp drivers/w5500_frame.cpp -o "$$output"; \
		"$$output"

.PHONY: test-ethernet-lifecycle
test-ethernet-lifecycle:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -Wall -Wextra -Itests/ethernet_stubs -I. tests/ethernet_lifecycle_test.cpp -o "$$output"; \
		"$$output"

.PHONY: test-ch390
test-ch390:
	@set -e; output=$$(mktemp); trap 'rm -f "$$output"' EXIT; \
		$(CXX) -std=gnu++14 -Wall -Wextra -I. tests/ch390_health_test.cpp -o "$$output"; \
		"$$output"; \
		$(CXX) -std=gnu++14 -Wall -Wextra -DARDUINO -DESP8266 -Itests/ch390_stubs -I. \
			tests/ch390_raw_test.cpp drivers/ch390_raw.cpp -o "$$output"; \
		"$$output"
