#!/bin/bash

echo "Building OpenSprinkler..."

if [ "$1" == "ospi" ]; then
	gcc -o OpenSprinkler -DOSPI main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp
elif [ "$1" == "osbo" ]; then
	gcc -o OpenSprinkler -DOSBO main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp
else
	g++ -o OpenSprinkler -DDEMO -m32 main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp
fi

./autoLaunch.sh
