#!/bin/bash

while getopts ":s" opt; do
  case $opt in
    s)
	  SILENT=true
	  command shift
      ;;
  esac
done
echo "Building OpenSprinkler..."

if [ "$1" == "demo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev
	echo "Compiling firmware..."
	g++ -o OpenSprinkler -DDEMO -std=c++14 -m32 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp sensors.cpp -lpthread -lmosquitto
elif [ "$1" == "osbo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev
	echo "Compiling firmware..."
	g++ -o OpenSprinkler -DOSBO main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp sensors.cpp -lpthread -lmosquitto
else
	echo "Installing required libraries..."
	apt-get update
	apt-get install -y libmosquitto-dev
	apt-get install -y raspi-gpio
	apt-get install -y libi2c-dev
	apt-get install -y libssl-dev
	apt-get install -y libgpiod-dev

	source build2.sh
fi

if [ ! "$SILENT" = true ] && [ -f OpenSprinkler.launch ] && [ ! -f /etc/systemd/system/OpenSprinkler.service ]; then

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
	sed -e 's,\_\_OpenSprinkler\_Path\_\_,'"$DIR"',g' OpenSprinkler.launch > OpenSprinkler.service

	# remove old start script: 
	service OpenSprinkler stop 2>/dev/null
	rm /etc/init.d/OpenSprinkler.sh 2>/dev/null

	# Move start up script to systemd directory
	sudo mv OpenSprinkler.service /etc/systemd/system/

	# Add to auto-launch on system startup
	systemctl daemon-reload
	systemctl enable OpenSprinkler.service

	# Start the deamon now
	service OpenSprinkler start

fi

echo "Done!"
