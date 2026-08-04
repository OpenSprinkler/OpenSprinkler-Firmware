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
    elif [[ $(grep -c '^dtparam=i2c_arm=on$' /boot/firmware/config.txt) -ge 1 ]] ; then
        echo "Setting the i2c clock speed to 400 kHz, you will have to reboot for this to take effect."
        sudo sed -i -e 's/dtparam=i2c_arm=on$/dtparam=i2c_arm=on,i2c_arm_baudrate=400000/g' /boot/firmware/config.txt
    fi
    else
		echo "Can not automatically enable i2c you might have to do this manually"
	fi
}

DEBUG=""
SILENT=false

function usage {
	echo "Usage: $0 [-s] [-d] [ospi|demo]"
}

while getopts ":sd" opt; do
  case $opt in
    s)
	  SILENT=true
      ;;
    d)
      DEBUG="-DENABLE_DEBUG -DSERIAL_DEBUG"
      ;;
	\?)
	  usage
	  exit 2
	  ;;
  esac
done
shift $((OPTIND - 1))

if [ "$#" -gt 1 ]; then
	usage
	exit 2
fi

TARGET="${1:-ospi}"
case "$TARGET" in
	demo)
		VERSION=DEMO
		;;
	ospi)
		VERSION=OSPI
		;;
	*)
		usage
		exit 2
		;;
esac

echo "Building OpenSprinkler..."

# Synchronize URLs and check out the exact revisions pinned by this firmware.
echo "Updating submodules."
git submodule sync --recursive
git submodule update --init --recursive --checkout

if [ "$TARGET" == "demo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev libssl-dev
else
	echo "Installing required libraries..."
	apt-get update
	# Switched from libgpiod-dev to liblgpio-dev
	apt-get install -y libmosquitto-dev libi2c-dev libssl-dev liblgpio-dev
    enable_i2c

fi

echo "Compiling $TARGET firmware..."
make clean
make VERSION="$VERSION" EXTRA_CXXFLAGS="$DEBUG"

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

    # Reload systemd
    systemctl daemon-reload

    # Enable and start the service
    systemctl enable OpenSprinkler
    systemctl start OpenSprinkler
fi

echo "Done!"
