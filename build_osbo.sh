# Build OpenSprinkler
echo "Building OpenSprinkler..."
gcc -o OpenSprinkler -DOSBO main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp

./autoLaunch.sh
