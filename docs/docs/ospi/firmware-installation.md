# Installing and Updating the OSPi Unified Firmware

## Background

This guide assumes you have a Raspberry Pi running Raspberry Pi OS, reachable over SSH or with a keyboard and monitor connected.

!!! note "Supported Raspberry Pi OS versions"
    Firmware 2.2.1(6) supports Raspberry Pi OS **Bookworm** and **Trixie** (either 64-bit or 32-bit). Older versions (e.g., Bullseye) may require installing the `lgpio` library manually. If your OS is very old (e.g., Buster), upgrade the OS before proceeding.

!!! warning "GPIO 4 conflict"
    Do **NOT** enable 1-wire on its default GPIO 4. OSPi uses GPIO 4 to control its shift register, so this conflict prevents it from operating stations correctly. If 1-wire was previously enabled, inspect `/etc/modules` with `sudo nano /etc/modules`, comment out any line containing `w1-gpio`, and reboot. If you must use 1-wire, configure it to use a different GPIO.

---

## New Installation of the Unified Firmware on a Raspberry Pi

1. Open an SSH connection to the Raspberry Pi (or connect a keyboard and monitor to the device).
2. Run the following commands:

        sudo apt-get install git
        cd ~
        git clone --recurse-submodules https://github.com/OpenSprinkler/OpenSprinkler-Firmware.git

3. Change the directory to the firmware folder:

        cd OpenSprinkler-Firmware

4. Build OpenSprinkler:

        sudo ./build.sh ospi

    * This generates an executable named `OpenSprinkler` in the firmware folder and offers to install a systemd service that starts OpenSprinkler at boot.
    * The script may enable I2C and set its bus speed to 400 kHz. Reboot the Raspberry Pi if the script says a reboot is required.
    * To compile the Demo target on a Debian-based Linux system, first run `sudo apt-get update`, then replace **ospi** with **demo**.

5. If you accepted the startup-service prompt, OpenSprinkler is now set up to run automatically. If you declined, run it manually with `sudo ./OpenSprinkler`, or rerun `sudo ./build.sh ospi` and accept the prompt.

6. On the Raspberry Pi itself, open <http://localhost:8080>. From another device on the same network, open `http://<pi-ip-address>:8080`. The default password is **opendoor**.

---

## Update a Previously Installed Unified Firmware

!!! warning
    **Always export your configuration using Export Configurations before updating the firmware.** A **Dotted Version Update** (for example, 2.2.0 → 2.2.1) resets the controller to factory defaults, including controller settings, programs, and the device password (which returns to the default: **opendoor**). A **Build Number Update**, such as 2.2.1(4) → 2.2.1(6), normally retains your settings, but a backup protects you if anything goes wrong.

1. Open an SSH connection to your Raspberry Pi.
2. Change to the firmware folder:

        cd ~/OpenSprinkler-Firmware

3. Run the updater:

        sudo ./updater.sh

    The updater performs a fast-forward-only Git pull, synchronizes the pinned submodules, rebuilds the firmware, and restarts the OpenSprinkler service.

### Manual Update

If `updater.sh` is unavailable or you need to troubleshoot an update, run these commands from `~/OpenSprinkler-Firmware`:

    git pull --ff-only
    sudo ./build.sh ospi
    sudo systemctl restart OpenSprinkler.service

To compile the Demo target on a Debian-based Linux system, first run `sudo apt-get update`, then replace **ospi** with **demo**. The Demo target does not install or restart the OSPi service.

The build script removes the legacy SysV init script when migrating an older installation. If `updater.sh` or `systemctl restart` reports that `OpenSprinkler.service` does not exist, rerun the interactive build and answer **Y** to **Do you want to start OpenSprinkler on startup?**:

    sudo ./build.sh ospi

---

## Stop All Sprinkler Firmware/Software from Starting with the Raspberry Pi

1. Open an SSH connection to your Raspberry Pi.

2. Stop the unified firmware and remove it from system startup:

        sudo systemctl disable --now OpenSprinkler.service
        sudo rm /etc/systemd/system/OpenSprinkler.service
        sudo systemctl daemon-reload

    Older installations may still use the legacy SysV init script. If `systemctl` reports that the service does not exist, use:

        sudo /etc/init.d/OpenSprinkler.sh stop
        sudo rm /etc/init.d/OpenSprinkler.sh
        sudo update-rc.d OpenSprinkler.sh remove

### Remove Legacy Sprinkler Software

Very old OSPi installations may also contain one of the following programs. Remove only the one that is actually installed.

**Dan's Python OSPi program:**

    sudo /etc/init.d/ospi stop
    sudo rm /etc/init.d/ospi
    sudo update-rc.d ospi remove

**Richard Zimmerman's `sprinklers_pi` program:**

    sudo /etc/init.d/sprinklers_pi stop
    sudo rm /etc/init.d/sprinklers_pi
    sudo update-rc.d sprinklers_pi remove
