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
	g++ -o OpenSprinkler -DDEMO -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto
elif [ "$1" == "osbo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev
	echo "Compiling firmware..."
	g++ -o OpenSprinkler -DOSBO -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto
else
	echo "Installing required libraries..."
	apt-get update
	apt-get install -y libmosquitto-dev
	apt-get install -y raspi-gpio
	if ! command -v raspi-gpio &> /dev/null
	then
		echo "Command raspi-gpio is required and is not installed"
		exit 0
	fi
	echo "Compiling firmware..."
	g++ -o OpenSprinkler -DOSPI -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto
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
