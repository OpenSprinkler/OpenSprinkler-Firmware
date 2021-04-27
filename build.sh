#!/bin/bash

# Default compiler
COMPILER=g++
COMPILERARGS=""

while g$COMPILERARGS etopts ":s:c:a:" opt; do
  case $opt in
    s)
	  SILENT=true
	  command shift
      ;;
	c)
	  COMPILER=${OPTARG}
	  ;;
	a)
	  COMPILERARGS=${OPTARG}
	  ;;
  esac
done
echo "Building OpenSprinkler..."

if [ "$1" == "demo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev
	echo "Compiling firmware..."
	$COMPILER -o OpenSprinkler -DDEMO $COMPILERARGS -m32 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto
elif [ "$1" == "osbo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev
	echo "Compiling firmware..."
	$COMPILER -o OpenSprinkler -DOSBO $COMPILERARGS main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto
else
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev
	echo "Compiling firmware..."
	$COMPILER -o OpenSprinkler -DOSPI $COMPILERARGS main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto
fi

# Exit with errorcode 1 on compiler errors
if [[ $? -ne 0 ]] ; then
    exit 1
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
