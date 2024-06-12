#!/bin/bash

function download_wiringpi {
	echo "Downloading WiringPi..."
	if [ $(arch) == "aarch64" ]; then
		wget https://github.com/WiringPi/WiringPi/releases/download/3.6/wiringpi_3.6_arm64.deb -O wiringpi.deb
	else
		wget https://github.com/WiringPi/WiringPi/releases/download/3.6/wiringpi_3.6_armhf.deb -O wiringpi.deb
	fi

	if [ $? -ne 0 ]; then
		echo "Failed to download WiringPi"
		exit 1
	fi

	echo "Installing WiringPi..."
	dpkg -i wiringpi.deb
	rm wiringpi.deb
}

function install_bcm2835 {
	echo "Installing bcm2835..."
	curl -sL http://www.airspayce.com/mikem/bcm2835/bcm2835-1.75.tar.gz | tar xz
	cd bcm2835-1.75
	./configure
	make
	sudo make install
	cd ..
	rm -rf bcm2835-1.75

}

function install_SSD1306_OLED_RPI {
	echo "Installing SSD1306_OLED_RPI..."
	curl -sL https://github.com/gavinlyonsrepo/SSD1306_OLED_RPI/archive/1.6.1.tar.gz | tar xz
	cd SSD1306_OLED_RPI-1.6.1
	make
	sudo make install
	cd ..
	rm -rf SSD1306_OLED_RPI-1.6.1
}

function enable_i2c {
	sudo raspi-config nonint do_i2c 1
	sudo dtparam i2c_baudrate=400000
	sudo modprobe i2c-dev
}

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

	if [ ! -f /usr/lib/libwiringPi.so ]; then
		download_wiringpi
	fi

	if [ ! -f /usr/local/lib/libbcm2835.a ]; then
		install_bcm2835
	fi

	if [ ! -f /usr/lib/libSSD1306_OLED_RPI.so ]; then
		install_SSD1306_OLED_RPI
	fi

	echo "Compiling demo firmware..."
	g++ -o OpenSprinkler -DDEMO -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto -lgpiod -lbcm2835 -lrt -lSSD1306_OLED_RPI
elif [ "$1" == "osbo" ]; then
	echo "Installing required libraries..."
	apt-get install -y libmosquitto-dev
	echo "Compiling osbo firmware..."
	g++ -o OpenSprinkler -DOSBO -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto
else
	echo "Installing required libraries..."
	apt-get update
	apt-get install -y libmosquitto-dev raspi-gpio libi2c-dev libssl-dev libgpiod-dev

	if ! command -v raspi-gpio &> /dev/null
	then
		echo "Command raspi-gpio is required and is not installed"
		exit 0
	fi

	if [ ! -f /usr/lib/libwiringPi.so ]; then
		download_wiringpi
	fi

	if [ ! -f /usr/local/lib/libbcm2835.a ]; then
		install_bcm2835
	fi

	if [ ! -f /usr/lib/libSSD1306_OLED_RPI.so ]; then
		install_SSD1306_OLED_RPI
	fi

	USEGPIO=""
	GPIOLIB=""


	if [ -h "/sys/class/gpio/gpiochip512" ]; then
		echo "using libgpiod"
		USEGPIO="-DLIBGPIOD"
		GPIOLIB="-lgpiod"
	fi

	echo "Compiling ospi firmware..."
	g++ -o OpenSprinkler -DOSPI $USEGPIO -std=c++14 main.cpp OpenSprinkler.cpp program.cpp opensprinkler_server.cpp utils.cpp weather.cpp gpio.cpp etherport.cpp mqtt.cpp -lpthread -lmosquitto -lgpiod -lbcm2835 -lrt -lSSD1306_OLED_RPI $GPIOLIB
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
