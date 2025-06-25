#!/bin/bash

	echo "Compiling OSPi firmware..."

	USEGPIO=""
        GPIOLIB=""

	source /etc/os-release
        if [[ $VERSION_ID -gt 10 ]]; then
                echo "using libgpiod"
                USEGPIO="-DLIBGPIOD"
                GPIOLIB="-lgpiod"
        fi


        ADS1115=""
        ADS1115FILES=""

        I2C=$(i2cdetect -y 1)
        if [[ "${I2C,,}" == *"48 --"* ]] ;then
                echo "found PCF8591"
                PCF8591="-DPCF8591"
                PCF8591FILES="./ospi-analog/driver_pcf8591*.c ./ospi-analog/iic.c"
        fi

        if [[ "${I2C,,}" == *"48 49"* ]] ;then
                echo "found ADS1115"
                ADS1115="-DADS1115"
                ADS1115FILES="./ospi-analog/driver_ads1115*.c ./ospi-analog/iic.c"
        fi


        echo "Compiling firmware..."
        ws=$(ls external/TinyWebsockets/tiny_websockets_lib/src/*.cpp)
        otf=$(ls external/OpenThings-Framework-Firmware-Library/*.cpp)
        ifx=$(ls external/influxdb-cpp/*.hpp)
        g++ -o OpenSprinkler -DOSPI $USEGPIO $ADS1115 $PCF8591 -DSMTP_OPENSSL $DEBUG -std=c++17 -include string.h main.cpp \
                OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp mqtt.cpp \
                smtp.c RCSwitch.cpp sensor*.cpp \
                $ADS1115FILES $PCF8591FILES \
                -Iexternal/TinyWebsockets/tiny_websockets_lib/include \
                $ws \
                -Iexternal/OpenThings-Framework-Firmware-Library/ \
                $otf \
                $ifx osinfluxdb.cpp -Iexternal/influxdb-cpp/ \
                -lpthread -lmosquitto -lssl -lcrypto -li2c -lmodbus $GPIOLIB


