#! /bin/bash

# Get latest release from GitHub api
#latest=$(curl --silent "https://api.github.com/repos/OpenSprinklerShop/OpenSprinkler-Firmware/releases/latest" | 
#    grep '"tag_name":' |                                            # Get tag line
#    sed -E 's/.*"([^"]+)".*/\1/')
#git switch --detach $latest    
git pull
./build.sh -s ospi
systemctl restart OpenSprinkler.service
