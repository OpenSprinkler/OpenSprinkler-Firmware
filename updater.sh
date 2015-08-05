#! /bin/bash

git pull
./build.sh ospi
/etc/init.d/OpenSprinkler.sh restart
