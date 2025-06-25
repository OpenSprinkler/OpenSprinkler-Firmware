#!/bin/bash
set -e

function enable_i2c {
    if command -v raspi-config &> /dev/null; then
    if [[ $(sudo raspi-config nonint get_i2c) -eq 1 ]] ; then
        echo "Enabling i2c"
        sudo modprobe i2c-dev
        sudo raspi-config nonint do_i2c 0
    fi
    if [[ $(grep -c '^dtparam=i2c_arm=on$' /boot/config.txt) -ge 1 ]] ; then
        echo "Setting the i2c clock speed to 400 kHz, you will have to reboot for this to take effect."
        sudo sed -i -e 's/dtparam=i2c_arm=on$/dtparam=i2c_arm=on,i2c_arm_baudrate=400000/g' /boot/config.txt
    fi
    else
		echo "Can not automatically enable i2c you might have to do this manually"
	fi
}

DEBUG=""

while getopts ":s:d" opt; do
  case $opt in
    s)
	  SILENT=true
	  command shift
      ;;
    d)
      DEBUG="-DENABLE_DEBUG -DSERIAL_DEBUG"
	  command shift
      ;;
  esac
done

FILENAME="sopts.dat"
if [[ ! "$SILENT" && -f "$FILENAME" ]];  then
	FILESIZE=$(stat -c%s "$FILENAME")
	if [[ "$FILESIZE" != "4160" ]]; then
		echo "This version has a new configuration data structure." 
		echo "Please backup configuration and restore after update!"
		echo "Otherwise your configuration is lost"
		echo "however, if this is a new installation, then you can proceed directly"
		read -p "Press ctrl-c to stop now or enter to continue"
	fi
fi

echo "Building OpenSprinkler..."

if [ -f /etc/init.d/OpenSprinkler.sh ]; then
	/etc/init.d/OpenSprinkler.sh stop
elif [ -f /etc/systemd/system/OpenSprinkler.service ]; then
	systemctl stop OpenSprinkler
fi

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
    	ifx=$(ls external/influxdb-cpp/*.cpp)
    	g++ -o OpenSprinkler -DDEMO -DSMTP_OPENSSL $DEBUG -std=c++14 -include string.h main.cpp \
    		OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp \
    		mqtt.cpp smtp.c RCSwitch.cpp \
    		-Iexternal/TinyWebsockets/tiny_websockets_lib/include \
    		$ws \
    		-Iexternal/OpenThings-Framework-Firmware-Library/ \
    		$otf \
    		$ifx \
    		-lpthread -lmosquitto -lssl -lcrypto
else
	echo "Installing required libraries..."
	apt-get update
	apt-get install -y libmosquitto-dev libi2c-dev libssl-dev libgpiod-dev gpiod libmodbus-dev
	enable_i2c

	./build2.sh
fi

if [ -f /etc/init.d/OpenSprinkler.sh ]; then
    echo "Detected the only init.d start up script, removing."
    echo "If you still want OpenSprinkler to launch on startup make sure when you run the build script to answer \"Y\" to the following question."
    /etc/init.d/OpenSprinkler.sh stop
    rm /etc/init.d/OpenSprinkler.sh
fi

if [ ! "$SILENT" = true ] && [ -f OpenSprinkler.service ] && [ -f startOpenSprinkler.sh ] && [ ! -f /etc/systemd/system/OpenSprinkler.service ]; then

	read -p "Do you want to start OpenSprinkler on startup? " -n 1 -r
	echo

	if [[ ! $REPLY =~ ^[Yy]$ ]]; then
		exit 0
	fi

	echo "Adding OpenSprinkler launch service..."

	# Get current directory (binary location)
	pushd "$(dirname $0)" > /dev/null
	DIR="$(pwd)"
	popd > /dev/null

	# Update binary location in start up script
	sed -e 's,\_\_OpenSprinkler\_Path\_\_,'"$DIR"',g' OpenSprinkler.service > /etc/systemd/system/OpenSprinkler.service

	# Make file executable
	chmod +x startOpenSprinkler.sh
fi
# Reload systemd
systemctl daemon-reload

# Enable and start the service
systemctl enable OpenSprinkler
systemctl start OpenSprinkler

echo "Done!"
