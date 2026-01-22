# Installing and Updating the OSPi Unified Firmware

## Background

This guide assumes you have a working Raspberry Pi setup with either SSH access available or a keyboard and monitor connected.

!!! note
    Do **NOT** enable 1-wire in raspi-config. The default 1-wire pin (GPIO 4) conflicts with OSPi and will prevent the RPi from sending signals correctly to OSPi. If you must use 1-wire, you can follow the RPi instructions to assign a different pin.

## New Installation of the Unified Firmware on a Raspberry Pi

!!! note
    The current firmware 2.2.1(4) supports both Raspberry Pi OS **Bookworm** and **Trixie** (either 64-bit or 32-bit). Older versions (e.g. Bullseye) may require installing the `lgpio` library manually. If your OS is very old (e.g. Buster), an upgrade should be made before proceeding.

1. Open an SSH connection to the Raspberry Pi (or connect a keyboard and monitor to the device).
2. Run the following command:

    ```
    sudo apt-get install git
    cd ~
    git clone --recurse-submodules https://github.com/OpenSprinkler/OpenSprinkler-Firmware.git
    ```

3. Change the directory to the firmware folder:

    ```
    cd OpenSprinkler-Firmware
    ```

4. Build OpenSprinkler:

    ```
    sudo ./build.sh ospi
    ```

    * This will generate an executable program called OpenSprinkler in the firmware folder, as well as setting up a script for auto-run at startup.
    * If you are using a BeagleBone Black, please replace **ospi** with **osbo**.
    * If you are compiling a demo to run on any Linux system, please replace **ospi** with **demo**.

5. If you answered yes to the startup script, you should be completely setup and ready.

    !!! note
        It has come to our attention that some Raspbian systems installed by NOOBs will take over GPIO 4 for 1-wire interface. However, OSPi needs GPIO 4 to send control signals to the solenoid valves. If you found that the firmware runs correctly but OSPi does not turn on valves correctly, one solution is to `sudo open /etc/modules`, and comment out the line containing `w1-gpio`, then reboot. Another solution is to reinstall Raspbian OS from scratch without using NOOBs.

6. The web interface should now be accessible from: <http://localhost:8080>. The default password to log into the web interface is **opendoor**.

## Update a Previously Installed Unified Firmware

!!! warning
    **The firmware update process will set your controller back to factory defaults.** This includes controller settings, program settings, and device password (which will be set back to the default: **opendoor**). Please ensure you backup your current configurations (e.g. Export Configurations) **before proceeding with a firmware update.**

!!! note
    The current firmware 2.2.1(4) supports both Raspberry Pi OS **Bookworm** and **Trixie** (either 64-bit or 32-bit). Older versions (e.g. Bullseye) may require installing the `lgpio` library manually. If your OS is very old (e.g. Buster), an upgrade should be made before proceeding.

1. Open an SSH connection to your Raspberry Pi.
2. Change directory to your firmware folder. By default it is:

    ```
    cd /home/pi/OpenSprinkler-Firmware
    ```

3. Update OpenSprinkler firmware source code from github:

    ```
    git fetch
    git pull --recurse-submodules
    ```

4. Re-build OpenSprinkler:

    ```
    sudo ./build.sh ospi
    ```

    * If you are using a BeagleBone Black, please replace **ospi** with **osbo**.
    * If you are compiling a demo to run on any Linux system, please replace **ospi** with **demo**.

5. Restart OpenSprinkler:

    ```
    sudo /etc/init.d/OpenSprinkler.sh restart
    ```

    More recent firmware versions have switched to use systemd. If the above command does not work, try the following:

    ```
    sudo systemctl restart OpenSprinkler.service
    ```

## Stop All Sprinler Firmware/Software from Starting with the Raspberry Pi

1. Open an SSH connection to your Raspberry Pi.

2. Remove unified firmware from system startup:

    ```
    sudo /etc/init.d/OpenSprinkler.sh stop
    sudo rm /etc/init.d/OpenSprinkler.sh
    sudo update-rc.d OpenSprinkler.sh remove
    ```

    More recent firmware versions have switched to use systemd. If the above commands do not work, try the following:

    ```
    sudo systemctl stop OpenSprinkler.service
    sudo rm /etc/systemd/system/OpenSprinkler.service
    ```

3. Remove Dan's Python OSPi program:

    ```
    sudo /etc/init.d/ospi stop
    sudo rm /etc/init.d/ospi
    sudo update-rc.d ospi remove
    ```

4. Remove Richard Zimmerman's sprinkler_pi program:

    ```
    sudo /etc/init.d/sprinklers_pi stop
    sudo rm /etc/init.d/sprinklers_pi
    sudo update-rc.d sprinklers_pi remove
    ```
