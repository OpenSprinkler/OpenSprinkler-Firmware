#!/usr/bin/env bash

UNAME_s=$(uname -s)

while getopts ":s" opt; do
  case $opt in
    s)
	  SILENT=true
	  command shift
      ;;
  esac
done
echo "Building OpenSprinkler..."

echo "Installing required libraries..."
case $UNAME_s in
FreeBSD)
	pkg install -y mosquitto
	;;
*)
	apt-get install -y libmosquitto-dev
	;;
esac

if [ "$1" == "demo" ]; then
	CFLAGS="-DDEMO -m32"
elif [ "$1" == "osbo" ]; then
	CFLAGS="-DOSBO"
else
	CFLAGS="-DOSPI"
fi
echo "Compiling firmware..."
SRCS="main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp"
if [ $UNAME_s = FreeBSD ]; then
	CXX=c++
	CFLAGS="$CFLAGS -I/usr/local/include -L/usr/local/lib"
else
	CXX=g++
fi
$CXX -o OpenSprinkler $CFLAGS $SRCS -lpthread -lmosquitto

if [ "$UNAME_s" != FreeBSD ] && [ ! "$SILENT" = true ] && [ -f OpenSprinkler.launch ] && [ ! -f /etc/init.d/OpenSprinkler.sh ]; then

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
