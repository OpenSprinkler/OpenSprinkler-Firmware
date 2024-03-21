#!/bin/bash

echo "Compiling OSPi firmware..."

ADS1115=""
ADS1115FILES=""
PCF8591=""
PCF8591FILES=""
USEGPIO=""
GPIOLIB=""
if [ -h "/sys/class/gpio/gpio??" ]; then
	echo "using raspi-gpio"
	USEGPIO="-DRASPIGPIO"
fi
if [ -h "/sys/class/gpio/gpiochip512" ]; then
	echo "using libgpiod"
	USEGPIO="-DLIBGPIOD"
	GPIOLIB="-lgpiod"
fi

i2cdetect -y 1 |grep "48 49" >/dev/null
if [ "$?" -eq 0 ]; then
	echo "found ADS1115"
	ADS1115="-DADS1115"
	ADS1115FILES="./ospi-analog/driver_ads1115*.c ./ospi-analog/iic.c"
fi
	
i2cdetect -y 1 |grep "48 --" >/dev/null
if [ "$?" -eq 0 ]; then
	echo "found PCF8591"
	PCF8591="-DPCF8591"
	PCF8591FILES="./ospi-analog/driver_pcf8591*.c ./ospi-analog/iic.c"
fi
	
g++ -g -o OpenSprinkler -DOSPI $ADS1115 $PCF8591 $USEGPIO -std=c++14 \
	main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp \
	utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp sensor*.cpp \
	$ADS1115FILES $PCF8591FILES \
	-li2c -lpthread -lmosquitto -lcrypto -lssl $GPIOLIB