#! /bin/bash

git pull
./build.sh -s ospi
systemctl restart OpenSprinkler.service
