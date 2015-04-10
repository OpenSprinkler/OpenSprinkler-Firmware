# Build OpenSprinkler
gcc -o OpenSprinkler -DOSPI main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp

# Get current directory (binary location)
pushd `dirname $0` > /dev/null
DIR=`pwd`
popd > /dev/null

# Update binary location in start up script
sed -i 's,\_\_OpenSprinkler\_Path\_\_,'"$DIR"',g' OpenSprinkler.sh

if [ ! -f /etc/init.d/OpenSprinkler.sh ]; then
	echo "Adding OpenSprinkler launch script..."

	# Move start up script to init.d directory
	mv OpenSprinkler.sh /etc/init.d/

	# Add to auto-launch on system startup
	update-rc.d OpenSprinkler.sh defaults

	# Start the deamon now
	/etc/init.d/OpenSprinkler.sh start

fi

echo "Done!"
