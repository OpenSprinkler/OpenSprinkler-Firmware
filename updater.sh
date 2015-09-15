#! /bin/bash

git pull
./build.sh -s ospi
/etc/init.d/OpenSprinkler.sh restart
