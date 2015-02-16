ARDUINO_VERS = 106
OFLAG = -Os

SKETCHBOOK = .
ARDUINO_DIR = /home/xxx/arduino-1.0.6
BOARD_TAG = os20
ARDUINO = $(ARDUINO_DIR)
AVR_TOOLS = $(ARDUINO_DIR)/hardware/tools/avr
PATH += $(ARDUINO)/hardware/tools/avr/bin
MONITOR_PORT = /dev/ttyUSB0

include $(SKETCHBOOK)/Arduino.mk
