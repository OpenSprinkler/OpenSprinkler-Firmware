#!/bin/bash
set -e

while getopts ":s" opt; do
  case $opt in
    s)
	  SILENT=true
	  command shift
      ;;
  esac
done
echo "Building OpenSprinkler..."

#Git update submodules

if git submodule status | grep --quiet '^-'; then
    echo "A git submodule is not initialized."
    git submodule update --recursive --init
else
    echo "Updating submodules."
    git submodule update --recursive
fi

if [ "$1" == "demo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev libssl-dev
	echo "Compiling demo firmware..."

    ws=$(ls external/TinyWebsockets/tiny_websockets_lib/src/*.cpp)
    otf=$(ls external/OpenThings-Framework-Firmware-Library/*.cpp)
    g++ -o OpenSprinkler -DDEMO -DSMTP_OPENSSL -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp mqtt.cpp smtp.c -Iexternal/TinyWebsockets/tiny_websockets_lib/include $ws -Iexternal/OpenThings-Framework-Firmware-Library/ $otf -lpthread -lmosquitto -lssl -lcrypto
elif [ "$1" == "osbo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev libssl-dev
	echo "Compiling osbo firmware..."

    ws=$(ls external/TinyWebsockets/tiny_websockets_lib/src/*.cpp)
    otf=$(ls external/OpenThings-Framework-Firmware-Library/*.cpp)
	g++ -o OpenSprinkler -DOSBO -DSMTP_OPENSSL -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp mqtt.cpp smtp.c -Iexternal/TinyWebsockets/tiny_websockets_lib/include $ws -Iexternal/OpenThings-Framework-Firmware-Library/ $otf -lpthread -lmosquitto -lssl -lcrypto
else
	echo "Installing required libraries..."
	apt-get update
	apt-get install -y libmosquitto-dev raspi-gpio libi2c-dev libssl-dev libgpiod-dev
	if ! command -v raspi-gpio &> /dev/null
	then
		echo "Command raspi-gpio is required and is not installed"
		exit 0
	fi

	USEGPIO=""
	GPIOLIB=""


	if [ -h "/sys/class/gpio/gpiochip512" ]; then
		echo "using libgpiod"
		USEGPIO="-DLIBGPIOD"
		GPIOLIB="-lgpiod"
	fi

	echo "Compiling ospi firmware..."
    ws=$(ls external/TinyWebsockets/tiny_websockets_lib/src/*.cpp)
    otf=$(ls external/OpenThings-Framework-Firmware-Library/*.cpp)
	g++ -o OpenSprinkler -DOSPI $USEGPIO -DSMTP_OPENSSL -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp mqtt.cpp smtp.c -Iexternal/TinyWebsockets/tiny_websockets_lib/include $ws -Iexternal/OpenThings-Framework-Firmware-Library/ $otf -lpthread -lmosquitto -lssl -lcrypto $GPIOLIB
fi

if [ ! "$SILENT" = true ] && [ -f OpenSprinkler.launch ] && [ ! -f /etc/init.d/OpenSprinkler.sh ]; then

	read -p "Do you want to start OpenSprinkler on startup? " -n 1 -r
	echo

	if [[ ! $REPLY =~ ^[Yy]$ ]]; then
		exit 0
	fi

	echo "Adding OpenSprinkler launch script..."

	# Get current directory (binary location)
	pushd `dirname $0` > /dev/null
	DIR=`pwd`
	popd > /dev/null

	# Update binary location in start up script
	sed -e 's,\_\_OpenSprinkler\_Path\_\_,'"$DIR"',g' OpenSprinkler.launch > OpenSprinkler.sh

	# Make file executable
	chmod +x OpenSprinkler.sh

	# Move start up script to init.d directory
	sudo mv OpenSprinkler.sh /etc/init.d/

	# Add to auto-launch on system startup
	sudo update-rc.d OpenSprinkler.sh defaults

	# Start the deamon now
	sudo /etc/init.d/OpenSprinkler.sh start

fi

echo "Done!"
