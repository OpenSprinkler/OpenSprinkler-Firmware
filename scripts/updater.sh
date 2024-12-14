#! /bin/bash

git pull
./scripts/build.sh -s ospi
systemctl restart OpenSprinkler.service
