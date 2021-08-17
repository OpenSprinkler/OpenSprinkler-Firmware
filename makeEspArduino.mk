#====================================================================================
# makeESPArduino
#
# A makefile for ESP8286 and ESP32 Arduino projects.
#
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2016-2021 Peter Lerup. All rights reserved.
#
#====================================================================================

START_TIME := $(shell date +%s)
__THIS_FILE := $(abspath $(lastword $(MAKEFILE_LIST)))
__TOOLS_DIR := $(dir $(__THIS_FILE))tools
OS ?= $(shell uname -s)

# Include possible operating system specfic settings
-include $(dir $(__THIS_FILE))/os/$(OS).mk

# Include possible global user settings
CONFIG_ROOT ?= $(if $(XDG_CONFIG_HOME),$(XDG_CONFIG_HOME),$(HOME)/.config)
-include $(CONFIG_ROOT)/makeEspArduino/config.mk

# Include possible project specific settings
-include $(firstword $(PROJ_CONF) $(dir $(SKETCH))config.mk)

# Build threads, default is using all the PC cpus
BUILD_THREADS ?= $(shell nproc)
MAKEFLAGS += -j $(BUILD_THREADS)

# Build verbosity, silent by default
ifndef VERBOSE
  MAKEFLAGS += --silent
endif

# ESP chip family type
CHIP ?= esp8266
UC_CHIP := $(shell perl -e "print uc $(CHIP)")
IS_ESP32 := $(if $(filter-out esp32,$(CHIP)),,1)

# Serial flashing parameters
UPLOAD_PORT_MATCH ?= /dev/ttyU*
UPLOAD_PORT ?= $(shell ls -1tr $(UPLOAD_PORT_MATCH) 2>/dev/null | tail -1)

# Monitor definitions
MONITOR_SPEED ?= 115200
MONITOR_PORT ?= $(UPLOAD_PORT)
MONITOR_PAR ?= --rts=0 --dtr=0
MONITOR_COM ?= $(if $(NO_PY_WRAP),python3,$(PY_WRAP)) -m serial.tools.miniterm $(MONITOR_PAR) $(MONITOR_PORT) $(MONITOR_SPEED)

# OTA parameters
OTA_ADDR ?=
OTA_PORT ?= $(if $(IS_ESP32),3232,8266)
OTA_PWD ?=
OTA_ARGS = --progress --ip="$(OTA_ADDR)" --port="$(OTA_PORT)"
ifneq ($(OTA_PWD),)
  OTA_ARGS += --auth="$(OTA_PWD)"
endif

# HTTP update parameters
HTTP_ADDR ?=
HTTP_URI ?= /update
HTTP_PWD ?= user
HTTP_USR ?= password
HTTP_OPT ?= --progress-bar -o /dev/null

# Output directory
BUILD_ROOT ?= /tmp/mkESP
BUILD_DIR ?= $(BUILD_ROOT)/$(MAIN_NAME)_$(BOARD)

# File system and corresponding disk directories
FS_TYPE ?= spiffs
FS_DIR ?= $(dir $(SKETCH))data
FS_RESTORE_DIR ?= $(BUILD_DIR)/file_system

# Utility functions
git_description = $(shell git -C  $(1) describe --tags --always --dirty 2>/dev/null || echo Unknown)
time_string = $(shell date +$(1))
find_files = $(shell find $2 | awk '/.*\.($1)$$/')

# ESP Arduino directories
ifndef ESP_ROOT
  # Location not defined, find and use possible version in the Arduino IDE installation
	ARDUINO_ROOT ?= $(HOME)/.arduino15
  ARDUINO_ESP_ROOT = $(ARDUINO_ROOT)/packages/$(CHIP)
  ESP_ROOT := $(if $(ARDUINO_HW_ESP_ROOT),$(ARDUINO_HW_ESP_ROOT),$(lastword $(wildcard $(ARDUINO_ESP_ROOT)/hardware/$(CHIP)/*)))
  ifeq ($(ESP_ROOT),)
    $(error No installed version of $(CHIP) Arduino found)
  endif
  ARDUINO_LIBS ?= $(shell grep -o "sketchbook.path=.*" $(ARDUINO_ROOT)/preferences.txt 2>/dev/null | cut -f2- -d=)/libraries
  ESP_ARDUINO_VERSION := $(notdir $(ESP_ROOT))
  # Find used version of compiler and tools
  COMP_PATH := $(lastword $(wildcard $(ARDUINO_ESP_ROOT)/tools/xtensa-*/*))
  MK_FS_PATH := $(lastword $(wildcard $(ARDUINO_ESP_ROOT)/tools/mk$(FS_TYPE)/*/mk$(FS_TYPE)))
  PYTHON3_PATH := $(lastword $(wildcard $(ARDUINO_ESP_ROOT)/tools/python3/*))
else
  # Location defined, assume that it is a git clone
  ESP_ARDUINO_VERSION = $(call git_description,$(ESP_ROOT))
  MK_FS_PATH := $(lastword $(wildcard $(ESP_ROOT)/tools/mk$(FS_TYPE)/mk$(FS_TYPE)))
	PYTHON3_PATH := $(wildcard $(ESP_ROOT)/tools/python3)
endif
ESP_LIBS = $(ESP_ROOT)/libraries
SDK_ROOT = $(ESP_ROOT)/tools/sdk
TOOLS_ROOT = $(ESP_ROOT)/tools

# The esp8266 tools directory contains the python3 executable as well as some modules
# Use these to avoid additional python installation requirements here
PYTHON3_PATH := $(if $(PYTHON3_PATH),$(PYTHON3_PATH),$(dir $(shell which python3 2>/dev/null)))
PY_WRAP = $(PYTHON3_PATH)/python3 $(__TOOLS_DIR)/py_wrap.py $(TOOLS_ROOT)
NO_PY_WRAP ?= $(if $(IS_ESP32),1,)

# Validate the selected version of ESP Arduino
ifeq ($(wildcard $(ESP_ROOT)/cores/$(CHIP)),)
  $(error $(ESP_ROOT) is not a vaild directory for $(CHIP))
endif

# Set possible default board variant and validate
BOARD_OP = perl $(__TOOLS_DIR)/board_op.pl $(ESP_ROOT)/boards.txt "$(CPU)"
ifeq ($(BOARD),)
  BOARD := $(if $(IS_ESP32),esp32,generic)
else ifeq ($(shell $(BOARD_OP) $(BOARD) check),)
  $(error Invalid board: $(BOARD))
endif

# Handle esptool variants
ESPTOOL_EXT = $(if $(IS_ESP32),,.py)
ESPTOOL ?= $(if $(NO_PY_WRAP),$(ESP_ROOT)/tools/esptool/esptool$(ESPTOOL_EXT),$(PY_WRAP) esptool)
ESPTOOL_COM ?= $(ESPTOOL) --baud=$(UPLOAD_SPEED) --port $(UPLOAD_PORT) --chip $(CHIP)
ifeq ($(IS_ESP32),)
  # esp8266, use esptool directly instead of via tools/upload.py in order to avoid speed restrictions currently implied there
  UPLOAD_COM = $(ESPTOOL_COM) $(UPLOAD_RESET) write_flash 0x00000 $(BUILD_DIR)/$(MAIN_NAME).bin
  FS_UPLOAD_COM = $(ESPTOOL_COM) $(UPLOAD_RESET) write_flash $(SPIFFS_START) $(FS_IMAGE)
endif

# Detect if the specified goal involves building or not
GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
BUILDING := $(if $(filter $(GOALS), monitor list_boards list_flash_defs list_lwip set_git_version install help tools_dir preproc info),,1)

# Sketch (main program) selection
ifeq ($(BUILDING),)
  SKETCH = /dev/null
endif
ifdef DEMO
  SKETCH := $(if $(IS_ESP32),$(ESP_LIBS)/WiFi/examples/WiFiScan/WiFiScan.ino,$(ESP_LIBS)/ESP8266WiFi/examples/WiFiScan/WiFiScan.ino)
endif
SKETCH ?= $(abspath $(wildcard *.ino *.pde))
ifeq ($(SKETCH),)
  $(error No sketch specified or found. Use "DEMO=1" for testing)
endif
ifeq ($(wildcard $(SKETCH)),)
  $(error Sketch $(SKETCH) not found)
endif
SRC_GIT_VERSION := $(call git_description,$(dir $(SKETCH)))

# Main output definitions
SKETCH_NAME := $(basename $(notdir $(SKETCH)))
MAIN_NAME ?= $(SKETCH_NAME)
MAIN_EXE ?= $(BUILD_DIR)/$(MAIN_NAME).bin
FS_IMAGE ?= $(BUILD_DIR)/FS.bin

# Build file extensions
OBJ_EXT = .o
DEP_EXT = .d

# Special tool definitions
OTA_TOOL ?= python $(TOOLS_ROOT)/espota.py
HTTP_TOOL ?= curl

# Core source files
CORE_DIR = $(ESP_ROOT)/cores/$(CHIP)
CORE_SRC := $(call find_files,S|c|cpp,$(CORE_DIR))
CORE_OBJ := $(patsubst %,$(BUILD_DIR)/%$(OBJ_EXT),$(notdir $(CORE_SRC)))
CORE_LIB = $(BUILD_DIR)/arduino.ar
USER_OBJ_LIB = $(BUILD_DIR)/user_obj.ar

# Find project specific source files and include directories
SRC_LIST = $(BUILD_DIR)/src_list.mk
FIND_SRC_CMD = $(__TOOLS_DIR)/find_src.pl
$(SRC_LIST): $(MAKEFILE_LIST) $(FIND_SRC_CMD) | $(BUILD_DIR)
	$(if $(BUILDING),echo "- Finding all involved files for the build ...",)
	perl $(FIND_SRC_CMD) "$(EXCLUDE_DIRS)" $(SKETCH) "$(CUSTOM_LIBS)" "$(LIBS)" $(ESP_LIBS) $(ARDUINO_LIBS) >$(SRC_LIST)

-include $(SRC_LIST)

# Use sketch copy with correct C++ extension
SKETCH_CPP = $(BUILD_DIR)/$(notdir $(SKETCH)).cpp
USER_SRC := $(subst $(SKETCH),$(SKETCH_CPP),$(USER_SRC))

USER_OBJ := $(patsubst %,$(BUILD_DIR)/%$(OBJ_EXT),$(notdir $(USER_SRC)))
USER_DIRS := $(sort $(dir $(USER_SRC)))

# Use first flash definition for the board as default
FLASH_DEF ?= $(shell $(BOARD_OP) $(BOARD) first_flash)
# Same method for LwIPVariant
LWIP_VARIANT ?= $(shell $(BOARD_OP) $(BOARD) first_lwip)

# Handle possible changed state i.e. make command line parameters or changed git versions
CMD_LINE ?= $(shell tr "\0" " " </proc/$$PPID/cmdline)
CMD_LINE := $(CMD_LINE)
IGNORE_STATE ?= $(if $(BUILDING),,1)
ifeq ($(IGNORE_STATE),)
  STATE_LOG := $(BUILD_DIR)/state.txt
  STATE_INF := $(strip $(foreach par,$(CMD_LINE),$(if $(findstring =,$(par)),$(par),))) \
               $(SRC_GIT_VERSION) $(ESP_ARDUINO_VERSION)
  # Ignore port and speed changes
  STATE_INF := $(patsubst UPLOAD_%,,$(STATE_INF))
  PREV_STATE_INF := $(if $(wildcard $(STATE_LOG)),$(shell cat $(STATE_LOG)),$(STATE_INF))
  ifneq ($(PREV_STATE_INF),$(STATE_INF))
    $(info * Build state has changed, doing a full rebuild *)
    $(shell rm -rf "$(BUILD_DIR)")
  endif
  STATE_SAVE := $(shell mkdir -p $(BUILD_DIR) ; echo '$(STATE_INF)' >$(STATE_LOG))
endif

# The actual build commands are to be extracted from the Arduino description files
ARDUINO_MK = $(BUILD_DIR)/arduino.mk
OS_NAME ?= linux
ARDUINO_DESC := $(shell find -L $(ESP_ROOT) -maxdepth 1 -name "*.txt" | sort)
$(ARDUINO_MK): $(ARDUINO_DESC) $(MAKEFILE_LIST) $(__TOOLS_DIR)/parse_arduino.pl | $(BUILD_DIR)
	$(if $(BUILDING),echo "- Parsing Arduino configuration files ...",)
	perl $(__TOOLS_DIR)/parse_arduino.pl $(BOARD) '$(FLASH_DEF)' '$(OS_NAME)' '$(LWIP_VARIANT)' $(ARDUINO_EXTRA_DESC) $(ARDUINO_DESC) >$(ARDUINO_MK)

-include $(ARDUINO_MK)

# Compilation directories and path
INCLUDE_DIRS += $(CORE_DIR) $(ESP_ROOT)/variants/$(INCLUDE_VARIANT) $(BUILD_DIR)
C_INCLUDES := $(foreach dir,$(INCLUDE_DIRS) $(USER_INC_DIRS),-I$(dir))
VPATH += $(shell find $(CORE_DIR) -type d) $(USER_DIRS)

# Automatically generated build information data source file
# Makes the build date and git descriptions at the time of actual build event
# available as string constants in the program
BUILD_INFO_H = $(BUILD_DIR)/buildinfo.h
BUILD_INFO_CPP = $(BUILD_DIR)/buildinfo.c++
BUILD_INFO_OBJ = $(BUILD_INFO_CPP)$(OBJ_EXT)
BUILD_DATE = $(call time_string,"%Y-%m-%d")
BUILD_TIME = $(call time_string,"%H:%M:%S")

$(BUILD_INFO_H): | $(BUILD_DIR)
	@echo "typedef struct { const char *date, *time, *src_version, *env_version; } _tBuildInfo; extern _tBuildInfo _BuildInfo;" >$@

# Use ccache if it is available and not explicitly disabled (USE_CCACHE=0)
USE_CCACHE ?= $(if $(shell which ccache 2>/dev/null),1,0)
ifeq ($(USE_CCACHE),1)
  C_COM_PREFIX = ccache
  CPP_COM_PREFIX = $(C_COM_PREFIX)
endif

# Generated header files
GEN_H_FILES += $(BUILD_INFO_H)

# Build output root directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Create a C++ file from the sketch
$(SKETCH_CPP): $(SKETCH)
	echo "#include <Arduino.h>" >$@
	cat $(abspath $<) >>$@

# Build rules for the different source file types
$(BUILD_DIR)/%.cpp$(OBJ_EXT): %.cpp $(ARDUINO_MK) | $(GEN_H_FILES)
	@echo  $(<F)
	$(CPP_COM) $(CPP_EXTRA) $(abspath $<) -o $@

$(BUILD_DIR)/%.c$(OBJ_EXT): %.c $(ARDUINO_MK) | $(GEN_H_FILES)
	@echo  $(<F)
	$(C_COM) $(C_EXTRA) $(abspath $<) -o $@

$(BUILD_DIR)/%.S$(OBJ_EXT): %.S $(ARDUINO_MK) | $(GEN_H_FILES)
	@echo  $(<F)
	$(S_COM) $(S_EXTRA) $(abspath $<) -o $@

$(CORE_LIB): $(CORE_OBJ)
	@echo Creating core archive
	rm -f $@
	$(CORE_LIB_COM) $^

$(USER_OBJ_LIB): $(USER_OBJ)
	@echo Creating object archive
	rm -f $@
	$(LIB_COM) cru $@ $^

# Possible user specific additional make rules
ifdef USER_RULES
include $(USER_RULES)
endif

# Putting the object files in a libarary minimizes the memory usage in the executable
ifneq ($(NO_USER_OBJ_LIB),)
  USER_OBJ_DEP = $(USER_OBJ)
else
  USER_OBJ_DEP = $(USER_OBJ_LIB)
endif

# Linking the executable
$(MAIN_EXE): $(CORE_LIB) $(USER_LIBS) $(USER_OBJ_DEP)
	@echo Linking $(MAIN_EXE)
	$(PRELINK)
	@echo "  Versions: $(SRC_GIT_VERSION), $(ESP_ARDUINO_VERSION)"
	@echo 	'#include <buildinfo.h>' >$(BUILD_INFO_CPP)
	@echo '_tBuildInfo _BuildInfo = {"$(BUILD_DATE)","$(BUILD_TIME)","$(SRC_GIT_VERSION)","$(ESP_ARDUINO_VERSION)"};' >>$(BUILD_INFO_CPP)
	$(CPP_COM) $(BUILD_INFO_CPP) -o $(BUILD_INFO_OBJ)
	$(LD_COM) $(LD_EXTRA)
	$(GEN_PART_COM)
	$(OBJCOPY)
	$(SIZE_COM) | perl $(__TOOLS_DIR)/mem_use.pl "$(MEM_FLASH)" "$(MEM_RAM)"
ifneq ($(LWIP_INFO),)
	@printf "LwIPVariant: $(LWIP_INFO)\n"
endif
ifneq ($(FLASH_INFO),)
	@printf "Flash size: $(FLASH_INFO)\n\n"
endif
	@perl -e 'print "Build complete. Elapsed time: ", time()-$(START_TIME),  " seconds\n\n"'

# Flashing operations
CHECK_PORT := $(if $(UPLOAD_PORT),\
                   @echo === Using upload port: $(UPLOAD_PORT) @ $(UPLOAD_SPEED),\
                   @echo "*** Upload port not found or defined" && exit 1)
upload flash: all
	$(CHECK_PORT)
	$(UPLOAD_COM)

ota: all
ifeq ($(OTA_ADDR),)
	@echo == Error: Address of device must be specified via OTA_ADDR
	exit 1
endif
	$(OTA_PRE_COM)
	$(OTA_TOOL) $(OTA_ARGS) --file="$(MAIN_EXE)"

http: all
ifeq ($(HTTP_ADDR),)
	@echo == Error: Address of device must be specified via HTTP_ADDR
	exit 1
endif
	$(HTTP_TOOL) $(HTTP_OPT) -F image=@$(MAIN_EXE) --user $(HTTP_USR):$(HTTP_PWD) http://$(HTTP_ADDR)$(HTTP_URI)
	@echo "\n"

$(FS_IMAGE): $(ARDUINO_MK) $(shell find $(FS_DIR)/ 2>/dev/null)
ifeq ($(SPIFFS_SIZE),)
	@echo == Error: No file system specified in FLASH_DEF
	exit 1
endif
	@echo Generating file system image: $(FS_IMAGE)
	$(MK_FS_COM)

fs: $(FS_IMAGE)

upload_fs flash_fs: $(FS_IMAGE)
	$(CHECK_PORT)
	$(FS_UPLOAD_COM)

ota_fs: $(FS_IMAGE)
ifeq ($(OTA_ADDR),)
	@echo == Error: Address of device must be specified via OTA_ADDR
	exit 1
endif
	$(OTA_TOOL) $(OTA_ARGS) --spiffs --file="$(FS_IMAGE)"

run: flash
	$(MONITOR_COM)

monitor:
ifeq ($(MONITOR_PORT),)
	@echo "*** Monitor port not found or defined" && exit 1
endif
	$(MONITOR_COM)

FLASH_FILE ?= $(BUILD_DIR)/esp_flash.bin
dump_flash:
	$(CHECK_PORT)
	@echo Dumping flash memory to file: $(FLASH_FILE)
	$(ESPTOOL_COM) read_flash 0 $(shell perl -e 'shift =~ /(\d+)([MK])/ || die "Invalid memory size\n";$$mem_size=$$1*1024;$$mem_size*=1024 if $$2 eq "M";print $$mem_size;' $(FLASH_DEF)) $(FLASH_FILE)

dump_fs:
	$(CHECK_PORT)
	@echo Dumping flash file system to directory: $(FS_RESTORE_DIR)
	-$(ESPTOOL_COM) read_flash $(SPIFFS_START) $(SPIFFS_SIZE) $(FS_IMAGE)
	mkdir -p $(FS_RESTORE_DIR)
	@echo
	@echo == Files ==
	$(RESTORE_FS_COM)

restore_flash:
	$(CHECK_PORT)
	@echo Restoring flash memory from file: $(FLASH_FILE)
	$(ESPTOOL_COM) -a soft_reset write_flash 0 $(FLASH_FILE)

erase_flash:
	$(CHECK_PORT)
	$(ESPTOOL_COM) erase_flash

# Building library instead of executable
LIB_OUT_FILE ?= $(BUILD_DIR)/$(MAIN_NAME).a
.PHONY: lib
lib: $(LIB_OUT_FILE)
$(LIB_OUT_FILE): $(filter-out $(BUILD_DIR)/$(MAIN_NAME).cpp$(OBJ_EXT),$(USER_OBJ))
	@echo Building library $(LIB_OUT_FILE)
	rm -f $(LIB_OUT_FILE)
	$(LIB_COM) cru $(LIB_OUT_FILE) $^

# Miscellaneous operations
clean:
	@echo Removing all build files
	rm -rf "$(BUILD_DIR)" $(FILES_TO_CLEAN)

list_boards:
	$(BOARD_OP) $(BOARD) list_names

list_lib: $(SRC_LIST)
	perl -e 'foreach (@ARGV) {print "$$_\n"}' "===== Include directories =====" $(USER_INC_DIRS)  "===== Source files =====" $(USER_SRC)

list_flash_defs:
	$(BOARD_OP) $(BOARD) list_flash

list_lwip:
	$(BOARD_OP) $(BOARD) list_lwip

# Update the git version of the esp Arduino repo
set_git_version:
ifeq ($(REQ_GIT_VERSION),)
	@echo == Error: Version tag must be specified via REQ_GIT_VERSION
	exit 1
endif
	@echo == Setting $(ESP_ROOT) to $(REQ_GIT_VERSION) ...
	git -C $(ESP_ROOT) checkout -fq --recurse-submodules $(REQ_GIT_VERSION)
	git -C $(ESP_ROOT) clean -fdxq -f
	git -C $(ESP_ROOT) submodule update --init
	git -C $(ESP_ROOT) submodule foreach -q --recursive git clean -xfd
	cd $(ESP_ROOT)/tools; ./get.py -q

# Generate a Visual Studio Code configuration and launch
BIN_DIR = /usr/local/bin
_MAKE_COM = make -f $(__THIS_FILE) ESP_ROOT=$(ESP_ROOT)
ifeq ($(CHIP),esp32)
  _MAKE_COM += CHIP=esp32
	_SCRIPT = espmake32
else
  _SCRIPT = espmake
endif
vscode: all
	perl $(__TOOLS_DIR)/vscode.pl -n $(MAIN_NAME) -m "$(_MAKE_COM)" -w "$(VS_CODE_DIR)" -i "$(VSCODE_INC_EXTRA)" -p "$(VSCODE_PROJ_NAME)" $(CPP_COM)

# Create shortcut command for running this file
install:
	@echo Creating command \"$(_SCRIPT)\" in $(BIN_DIR)
	sudo sh -c 'echo $(_MAKE_COM) "\"\$$@\"" >$(BIN_DIR)/$(_SCRIPT)'
	sudo chmod +x $(BIN_DIR)/$(_SCRIPT)

# Just return the path of the tools directory (intended to be used to find vscode.pl above from othe makefiles)
tools_dir:
	@echo $(__TOOLS_DIR)

# Show ram memory usage per variable
ram_usage: $(MAIN_EXE)
	$(shell find $(TOOLS_ROOT) | grep 'gcc-nm') -Clrtd --size-sort $(BUILD_DIR)/$(MAIN_NAME).elf | grep -i ' [b] '

# Show ram and flash usage per object files used in the build
OBJ_INFO_FORM ?= 0
OBJ_INFO_SORT ?= 1
obj_info: $(MAIN_EXE)
	perl $(__TOOLS_DIR)/obj_info.pl "$(shell find $(TOOLS_ROOT) | grep 'elf-size$$')" "$(OBJ_INFO_FORM)" "$(OBJ_INFO_SORT)" $(BUILD_DIR)/*.o

# Analyze crash log
crash: $(MAIN_EXE)
	perl $(__TOOLS_DIR)/crash_tool.pl $(ESP_ROOT) $(BUILD_DIR)/$(MAIN_NAME).elf

# Run compiler preprocessor to get full expanded source for a file
preproc:
ifeq ($(SRC_FILE),)
	$(error SRC_FILE must be defined)
endif
	$(CPP_COM) -E $(SRC_FILE)

# Main default rule, build the executable
.PHONY: all
all: $(BUILD_DIR) $(ARDUINO_MK) prebuild $(MAIN_EXE)

# Prebuild is currently only mandatory for esp32
USE_PREBUILD ?= $(if $(IS_ESP32),1,)
prebuild:
ifneq ($(USE_PREBUILD),)
	$(PREBUILD)
endif

help: $(ARDUINO_MK)
	@echo
	@echo "Generic makefile for building Arduino esp8266 and esp32 projects"
	@echo "This file can either be used directly or included from another makefile"
	@echo ""
	@echo "The following targets are available:"
	@echo "  all                  (default) Build the project application"
	@echo "  clean                Remove all intermediate build files"
	@echo "  lib                  Build a library with all involved object files"
	@echo "  flash                Build and and flash the project application"
	@echo "  flash_fs             Build and and flash file system (when applicable)"
	@echo "  ota                  Build and and flash via OTA"
	@echo "                         Params: OTA_ADDR, OTA_PORT and OTA_PWD"
	@echo "  ota_fs               Build and and flash file system via OTA"
	@echo "  http                 Build and and flash via http (curl)"
	@echo "                         Params: HTTP_ADDR, HTTP_URI, HTTP_PWD and HTTP_USR"
	@echo "  dump_flash           Dump the whole board flash memory to a file"
	@echo "  restore_flash        Restore flash memory from a previously dumped file"
	@echo "  dump_fs              Extract all files from the flash file system"
	@echo "                         Params: FS_DUMP_DIR"
	@echo "  erase_flash          Erase the whole flash (use with care!)"
	@echo "  list_lib             Show a list of used solurce files and include directories"
	@echo "  set_git_version      Setup ESP Arduino git repo to a the tag version"
	@echo "                         specified via REQ_GIT_VERSION"
	@echo "  install              Create the commands \"espmake\" and \"espmake32\""
	@echo "  vscode               Create config file for Visual Studio Code and launch"
	@echo "  ram_usage            Show global variables RAM usage"
	@echo "  obj_info             Show memory usage per object file"
	@echo "  monitor              Start serial monitor on the upload port"
	@echo "  run                  Build flash and start serial monitor"
	@echo "  crash                Analyze stack trace from a crash"
	@echo "  preproc              Run compiler preprocessor on source file"
	@echo "                         specified via SRC_FILE"
	@echo "  info                 Show location and version of used esp Arduino"
	@echo "Configurable parameters:"
	@echo "  SKETCH               Main source file"
	@echo "                         If not specified the first sketch in current"
	@echo "                         directory will be used."
	@echo "  LIBS                 Use this variable to declare additional directories"
	@echo "                         and/or files which should be included in the build"
	@echo "  CHIP                 Set to esp8266 or esp32. Default: '$(CHIP)'"
	@echo "  BOARD                Name of the target board. Default: '$(BOARD)'"
	@echo "                         Use 'list_boards' to get list of available ones"
	@echo "  FLASH_DEF            Flash partitioning info. Default '$(FLASH_DEF)'"
	@echo "                         Use 'list_flash_defs' to get list of available ones"
	@echo "  BUILD_DIR            Directory for intermediate build files."
	@echo "                         Default '$(BUILD_DIR)'"
	@echo "  BUILD_EXTRA_FLAGS    Additional parameters for the compilation commands"
	@echo "  COMP_WARNINGS        Compilation warning options. Default: $(COMP_WARNINGS)"
	@echo "  FS_TYPE              File system type. Default: $(FS_TYPE)"
	@echo "  FS_DIR               File system root directory"
	@echo "  UPLOAD_PORT          Serial flashing port name. Default: '$(UPLOAD_PORT)'"
	@echo "  UPLOAD_SPEED         Serial flashing baud rate. Default: '$(UPLOAD_SPEED)'"
	@echo "  MONITOR_SPEED        Baud rate for the monitor. Default: '$(MONITOR_SPEED)'"
	@echo "  FLASH_FILE           File name for dump and restore flash operations"
	@echo "                          Default: '$(FLASH_FILE)'"
	@echo "  LWIP_VARIANT         Use specified variant of the lwip library when applicable"
	@echo "                         Use 'list_lwip' to get list of available ones"
	@echo "                         Default: $(LWIP_VARIANT) ($(LWIP_INFO))"
	@echo "  VERBOSE              Set to 1 to get full printout of the build"
	@echo "  BUILD_THREADS        Number of parallel build threads"
	@echo "                         Default: Maximum possible, based on number of CPUs"
	@echo "  USE_CCACHE           Set to 0 to disable ccache when it is available"
	@echo "  NO_USER_OBJ_LIB      Set to 1 to disable putting all object files into an archive"
	@echo

# Show installation information
info:
	echo == Build info
	echo "  CHIP:        $(CHIP)"
	echo "  ESP_ROOT:    $(ESP_ROOT)"
	echo "  Version:     $(ESP_ARDUINO_VERSION)"
	echo "  Threads:     $(BUILD_THREADS)"
	echo "  Upload port: $(UPLOAD_PORT)"

# Include all available dependencies from the previous compilation
-include $(wildcard $(BUILD_DIR)/*$(DEP_EXT))

DEFAULT_GOAL ?= all
.DEFAULT_GOAL := $(DEFAULT_GOAL)

