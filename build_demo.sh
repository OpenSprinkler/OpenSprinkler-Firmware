# Build OpenSprinkler
echo "Building OpenSprinkler..."
g++ -o OpenSprinkler -DDEMO -m32 main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp

./autoLaunch.sh
