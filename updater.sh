#! /bin/bash

git pull
./build.sh -s ospi
service OpenSprinkler restart
