gcc -o OpenSprinkler -DOSPI main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp gpio.cpp
sed -i "s/\<\%OpenSprinkler_Path\%\>/${PWD##*/}/g" OpenSprinkler.sh
mv OpenSprinkler.sh /etc/init.d/
update-rc.d OpenSprinkler defaults
/etc/init.d/OpenSprinkler start
