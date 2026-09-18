# Firmware Update

## OpenSprinkler v3 and v4 {: .hltitle}

Firmware updates use the controller's local update page. Open it through the controller's IP address, not through an OpenThings Cloud (OTC) connection. The update page uses port `8080` for uploads, so that port must be reachable from your browser.

!!! warning "Back Up Before Updating"
    Before update, please back up your configurations (Sidebar → **Export Configuration**) so you can quickly restore programs and settings if needed.

    * A **Dotted Version Update**, e.g. 2.2.0 to 2.2.1, triggers a factory reset and **erases all programs, settings, and logs**.
    * A **Build Number Update**, e.g. 2.2.1(5) to 2.2.1(6), normally preserves programs and settings. On v3, the 2.2.1(6) upgrade removes old sprinkler logs while migrating to the new bounded log format.

1. **Open the Firmware Update page:** From the controller's local homepage, open the side menu and choose **Update Firmware**, or go directly to `http://os-ip/update` (replace `os-ip` with the controller's IP address).
2. **Choose the firmware:** On 2.2.1(6) or later, select an available release and follow the prompts. The page downloads the image; the controller verifies its signature and checksum before installing it. A controller running older firmware needs a manual upload to reach 2.2.1(6) first.
3. **Wait for completion:** Keep the controller powered and the browser open until the update finishes and the controller reboots.

### Manual Upload

If the controller is running older firmware or cannot use the online release list, download the correct 2.2.1(6) image and select it with the file upload control on the local update page:

* [OpenSprinkler v3 (ESP8266): `.bin`](https://firmware.opensprinkler.com/v1/releases/2.2.1-6/opensprinkler-2.2.1-6-esp8266.bin)
* [OpenSprinkler v4 (ESP32-C6-N8): `.bin32n8`](https://firmware.opensprinkler.com/v1/releases/2.2.1-6/opensprinkler-2.2.1-6-esp32c6.bin32n8)

Select the image for your hardware and enter the controller password when prompted. Do not install a v3 image on v4 or a v4 image on v3.

---

### Alternative Methods to Open the Firmware Update Page

* **WiFi AP mode:** If your controller is currently in WiFi AP mode, connect your computer or phone to the AP SSID shown on the controller's LCD, then open `http://192.168.4.1/update` in a browser.

---

### Troubleshooting

* **Blank Homepage:** If the device homepage is blank or showing an error, and you need to export configuration before updating, see [Blank Page Troubleshooting](troubleshooting.md#ui-app-time-and-lcd).
* **No Available Firmware:** Use the manual upload if your browser cannot reach the firmware release site, or if the controller's firmware predates the online release list.
* **Upload Connection Failure:** Make sure port `8080` is not blocked by your computer, router, or firewall, then retry the upload.
* **Controller Stops Responding:** Unplug and reconnect power, then retry the update.
* **Update via Wired Ethernet:** If your controller is connected via wired Ethernet:
    * If it runs firmware `2.2.0` or newer, follow the same steps as WiFi.
    * If it runs `2.1.9` or earlier, update must be done in WiFi mode:
        1. Power off the controller.
        2. Remove the Ethernet module.
        3. Power on - it will boot into WiFi AP mode.
        4. Follow the AP-mode update instructions above.
* **AP Mode Password Issues on Legacy Firmware:** Some older AP-mode update pages do not hash the password before submitting it. If your plaintext password fails, try its **MD5 hash** instead (e.g. the MD5 hash of `opendoor` is `a6d82bced638de3def1e9bbb4983225c`).
* **Firmware Corruption / Device Not Booting**: If device fails to boot after an update, you'll need to re-flash the firmware using a [USB-Serial Programmer](https://opensprinkler.com/product/usb-programmer/).

<hr class="double">

## OpenSprinkler v2.3 {: .hltitle}

Requires a USB cable for firmware update. Follow the legacy [v2.3 Firmware Update Instructions](https://openthings.freshdesk.com/a/solutions/articles/5000832311).

<hr class="double">

## OpenSprinkler Pi (OSPi) {: .hltitle}

Update is done directly on the RPi. Follow [OSPi Firmware Update Instructions](https://openthings.freshdesk.com/a/solutions/articles/5000631599).

<hr class="double">
