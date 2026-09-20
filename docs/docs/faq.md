## OpenSprinkler Frequently Asked Questions

!!! note
    This page covers the most common questions. For technical support, check the [User Manual](manual.md), the [Support Portal](https://support.opensprinkler.com) or the [Community Forums](https://opensprinkler.com/forums/).

### Pre-Sales

**Q: What is OpenSprinkler?**
<br>
OpenSprinkler is an open-source, web-based sprinkler and irrigation controller. It's a drop-in replacement for conventional sprinkler controllers, offering several advantages:

* **Intuitive User Interface (UI):** No more messing with buttons and knobs. Use web browsers and our free mobile apps to access OpenSprinkler.
* **Remote Access:** Access your OpenSprinkler from anywhere, whether you are at home, at the office, or traveling.
* **Smart Control:** OpenSprinkler can connect to the Internet and use current, historical, and forecast weather data to automatically adjust watering times. It not only stops watering when it rains but also scales watering time up or down based on your local temperature and humidity.
* **Connectivity Options:** OpenSprinkler supports both WiFi and wired Ethernet. It follows a **local-first approach** — it works directly on your local network without Internet access, while optional cloud access enables remote control. Its built-in WiFi can also function as an Access Point (AP) when no external router is available.
* **Expandable:** The main controller handles 8 zones and is expandable up to 72 zones with expanders. Our app can also manage multiple OpenSprinkler systems for limitless expansion.
* **Advanced Features:** OpenSprinkler supports features rarely seen on competing products, such as simultaneous zone runs, MQTT, remote zones, HTTP(S) zones, email notifications, radio frequency (RF) stations, multiple sensor types including flow and analog sensors, four independent master stations, repeating program start times, and multi-language support.

---

**Q: Who is OpenSprinkler for?**
<br>
Homeowners and business owners, including growers, farms, business parks, ranches, golf courses, and sprinkler service professionals. It's also for electronics enthusiasts who like to tinker and use OpenSprinkler as a platform for their own projects. The system scales from a few zones to large multi-zone and multi-controller sites.

---
<a id="hardware-versions"></a>
**Q: What hardware versions are available?**
<br>
We currently offer **OpenSprinkler v3 and v4** (WiFi with an optional wired Ethernet module) and **OpenSprinkler Pi** (OSPi, driven by Raspberry Pi). OpenSprinkler v3 is available in three variants:

* **AC-powered:** Operates standard 24VAC sprinkler valves and is powered by a 24VAC adapter.
* **DC-powered:** Also operates 24VAC sprinkler valves, but is powered by a DC adapter. This is convenient for international customers or for off-grid / solar-powered setups.
* **Latch:** Powered by a DC adapter and operates <u>latching solenoids</u> only.

OpenSprinkler v4 and OSPi are currently available only in the AC-powered variant.

---
<a id="choose-models"></a>
**Q: How do I choose between the AC/DC/Latch models?**

* **Using a pump start relay?** Choose the AC model: it supports all 24VAC pump relays.
    * If you prefer the DC model but need to control a pump, the best option is to use a solid state relay (SSR) or DC-input relay.
* **Using latching solenoids?** Choose the Latch model: it's the only model that works with latching valves (often labeled 'Latch/Latching' on the valve body).
* **Otherwise**: Choose either the AC or DC model.
    * Both work with standard 24VAC valves.
    * DC is more flexible and supports off-grid/solar-powered setups. In addition, DC adapters are easier and cheaper to source outside North America.
    * AC is the simplest drop-in if you already have a 24VAC transformer or accessories (e.g. pump relays, wireless sensors) that require 24VAC.

---

**Q: How many zones does it support?**
<br>
The main controller supports `8` zones; each expander adds `16`. Current firmware supports a maximum of `72` zones on OS v3 and v4, and `200` on OSPi.

---

**Q: I have 12 zones. What do I need to buy?**
<br>
One main controller (`8` zones) and one expander (`16` additional) give you a total of `24` zones, covering all `12` of your zones with room for future expansion. OpenSprinkler uses a software-defined master zone, so each master counts as a separate zone (e.g. `12` zones plus a master count as `13` zones).

---

**Q: How many programs and start times are supported?**
<br>
Up to `40` programs. Each program supports per-station durations (`0–18` h, **one-second precision**); weekly, monthly, single-run, or interval schedule dates (e.g. every `n` days); and either up to `4` arbitrary fixed start times, or repeating start times with any number of repeats (e.g., start `8:30 AM`, repeat every `8` min for `55` times).

---

**Q: Does the weather feature work outside North America?**
<br>
Yes. You can choose among a variety of weather data providers, including Apple Weather, Open-Meteo, AccuWeather, and Weather Underground, most of which are available worldwide.

---

**Q: Does it require a cloud connection?**
<br>
No. OpenSprinkler is **local-first** — by default it connects only to your local network and does NOT rely on the Internet. Cloud connection is optional and only needed for remote access.

---

**Q: Is there a subscription fee for cloud access, app, or weather data?**
<br>
No. OpenSprinkler is a one-time purchase. All features are **completely free** without a recurring fee.

---

**Q: What WiFi network is it compatible with?**
<br>
OS v3 and v4 are compatible with 2.4GHz WiFi. They do **NOT** currently support 5GHz. Ensure your router is broadcasting a 2.4GHz network (most 5GHz routers support dual-band operation and can broadcast both).

---

**Q: How is OpenSprinkler different from competitors?**

* It has a built-in web interface for local control, and runs programs on its own, without relying on proprietary or cloud-only software.
* Advanced features, such as second-precision watering times, simultaneous zone runs, multiple onboard sensors and master zones, pause, support for flow and analog sensors, email notifications, and special station types (Remote, RF, HTTP(S), and Bundle Station).
* Supports both WiFi and wired Ethernet options.
* Easy expansion to `72` zones (or `200` with OSPi) at a much lower cost than competitors.
* Cloud access is optional: your controller works locally even if your internet is down.
* Built on **open-source hardware and software**: its design files are public, allowing for customization and extension.

---

**Q: Where are OpenSprinkler's documents and source code?**

* [User Manual](manual.md)
* [API Reference](api.md)
* [Video Tutorials](https://openthings.freshdesk.com/support/solutions/articles/5000860920-videos-introduction-to-opensprinkler-v3)
* [GitHub Repository](https://github.com/opensprinkler)

<hr class="double">

### Shipping

**Q: Where can I buy it?**
<br>
You can purchase it directly from the [OpenSprinkler product page](https://opensprinkler.com/product/opensprinkler/).

---

**Q: What's included in the package?**

* One OpenSprinkler controller (8 zones).
* **AC-powered** model: 24VAC adapter **not** included by default. It can be purchased as an optional add-on (for orders shipped to North America), or you can reuse an existing sprinkler transformer (22-28VAC).
* **DC-powered** & **Latch** models: include a compatible universal DC power adapter for all orders.

---

**Q: How fast do you ship?**
<br>
Typically within 1 business day (unless noted otherwise or back-ordered). Tracking number is emailed to you automatically once shipped.

---

**Q: What shipping options are available? What’s the warranty/return policy?**
<br>
Please check our [Terms and Conditions](https://opensprinkler.com/terms-and-conditions/).

<hr class="double">

### Installation and Usage

**Q: I'm new to sprinkler systems. How do I install and wire it?**
<br>
OpenSprinkler is a drop-in replacement for an existing controller. DIY installation is easy.

*  Carefully label and remove wires from your existing controller.
*  Mount OpenSprinkler and re-insert wires.
*  Connect to your router (WiFi or wired).
*  Follow the [User Manual — Installation](manual.md#installation) section for detailed instructions.
*  Tutorial videos are also available on our [Support page](https://support.opensprinkler.com) if you prefer video guidance.

If you are installing a new system with no labeled wires, note that each valve has two wires: one to **COM** (common), and the other to a zone terminal.

---

**Q: Is the controller waterproof?**
<br>
No. For outdoor installs, use a waterproof enclosure such as [this one](https://www.amazon.com/Orbit-57095-Weather-Resistant-Outdoor-Mounted-Controller/dp/B000VYGMF2).

---
<a id="supported-valves"></a>
**Q: What valves are supported?**
<br>
* **AC-powered (including OSPi):** Standard 24VAC sprinkler valves, motorized ball valves (rated for 24VAC), pump start relays, and wireless sensors that use 24VAC.
* **DC-powered:** Standard 24VAC sprinkler valves, DC **non-latching** valves, motorized ball valves (rated for DC), and DC solid state relays.
* **Latch OpenSprinkler:** Latching solenoid valves only, which typically have two wires with distinct colors (e.g. black and red).

---

**Q: Are motorized ball valves supported?**
<br>
Yes. The **2-wire auto return** and **2-wire 9-24V AC/DC** types are directly compatible. The **3-wire 9-24V AC/DC** type is also compatible, but each valve will take 2 zones (for open and close).

---

**Q: Does it support master / pump stations?**
<br>
Yes: up to **four** independent, software-defined master/pump zones.

---

**Q: Can I connect sprinkler valves with garden hoses?**
<br>
Yes. Use an **NPT** (National Pipe Thread) to **GHT** (Garden Hose Thread) adapter.

---

**Q: Can I put two wires in one zone port?**
<br>
Yes, but keep in mind that those two zones will always operate together.

---

**Q: What sensors are supported?**
<br>
OpenSprinkler v3.0-3.3 has two built-in sensor ports; v3.4 and v4 have four. Each port can be independently configured as a:

* **Rain** sensor (normally open or normally closed)
* **Flow** sensor (two-wire dry-contact; and some three-wire types)
* **Digital soil moisture** sensor (outputs binary signal)
* **Program start button**

[Expanded Sensors](sensor-expander.md) additionally include analog sensors and Weather Sensors, along with options to combine readings or monitor system information. Their values can be displayed, logged, and used to adjust program watering.

---

**Q: Do you support analog soil sensors?**
<br>
Yes. OpenSprinkler v3 and v4 can use the [Sensor Expander](sensor-expander.md), which provides 16 analog input channels. Recent OSPi versions include eight analog inputs through two onboard ADS1115 chips. You can also use our [Analog-to-Digital (A2D) adapter](https://opensprinkler.com/product/a2dadapter/) to convert an analog sensor output into a digital on/off signal.

---

**Q: What is the maximum distance between the controller and valves?**
<br>
Depends on the wire gauge (AWG) you use.

* **20 AWG:** Up to 200 m (700 ft)
* **18 AWG:** Up to 300 m (1000 ft)
* **16 AWG:** Up to 450 m (1500 ft)

---

**Q: What is the maximum distance between the main controller and extension boards?**
<br>
Depends on the wire gauge of the extension cable. Default expander cable is 24 AWG at 15 in; longer runs are possible with custom cabling (e.g. by using an Ethernet cable).

---

**Q: What are the LCD and buttons for?**
<br>
LCD shows time as well as zone/controller/sensor status. Buttons can show IP, perform factory reset, and start programs manually.

---

**Q: How do I check its IP address?**
<br>
Press button `B1` to display the IP.

---

**Q: How do I start a program using buttons?**
<br>
Press and hold button `B3` until “Run a Program” appears on the LCD, then follow prompts.

---

**Q: What happens if I lose power?**
<br>
Programs and settings are stored in non-volatile (flash) memory and are preserved during outages.

---

**Q: Can I run multiple zones simultaneously (concurrently)?**
<br>
Yes. You can assign zones to the **Parallel (P)** group or use a **Bundle Station** to activate selected zones together. The maximum number of simultaneous zones is not limited by software, but rather depends on the power capacity of your adapter and the current draw of your valves. Up to `8` have been tested internally. We generally recommend no more than `4`.

---

**Q: Can OpenSprinkler switch other devices?**
<br>
Yes. The firmware supports GPIO/HTTP(S)/RF stations. This allows it to toggle a GPIO pin, send an HTTP(S) command, or talk to RF remote power sockets, which can switch devices like lights, heaters, pumps, and fans. You can also use 24VAC relays to allow switching other devices.

---

**Q: Does it have overcurrent / undercurrent detection?**
<br>
Yes. Current firmware supports adjustable overcurrent and undercurrent thresholds in the settings. The overcurrent limit stops zones when the current draw is too high, while the undercurrent alert helps identify zones drawing less current than expected. Both help detect faulty valves and wiring.

<hr class="double">

### Web Connections and Integration

**Q: Does it have built-in wireless? What about wired Ethernet?**
<br>
Yes, OpenSprinkler v3 and v4 have built-in WiFi. A wired Ethernet module is an optional add-on. OpenSprinkler Pi's network options depend on your Raspberry Pi.

---

**Q: Do I need a WiFi router?**
<br>
Typically you connect the controller to a router (WiFi or Ethernet). However, without a router, OS v3 and v4 can run in **AP mode** (acts as its own hotspot). Internet-dependent features (like weather) won’t work in pure AP mode.

---

**Q: How do I access OpenSprinkler?**
<br>
OpenSprinkler has a built-in web interface that works with any modern desktop or mobile browser. Simply type the controller's IP address into your browser. We also provide a free OpenSprinkler mobile app (for iOS, Android, and macOS).

For remote access, configure an **OTC (OpenThings Cloud)** token in the settings.

---

**Q: Can OpenSprinkler run offline without Internet?**
<br>
Yes. Once programmed, OpenSprinkler runs all schedules offline. The controller has a built-in real-time clock and battery for timekeeping. You can also use the onboard buttons to run programs manually.

---

**Q: Do you support push notifications?**
<br>
Yes. Current firmware supports notifications via email, MQTT, and IFTTT (see [support documentation](https://openthings.freshdesk.com/solution/folders/5000099525)).

---

**Q: Can multiple OpenSprinklers talk to each other?**
<br>
Yes. The firmware supports "Remote Stations", a feature that allows one OpenSprinkler to act as a master controller, sending commands to other controllers to open or close their valves.

<hr class="double">

### Technical

**Q: What are the differences between the product versions?**
<br>
OpenSprinkler v3 and v4 are fully assembled and work out of the box. OpenSprinkler Pi (OSPi) is based on Raspberry Pi (RPi) and requires experience with RPi and basic Linux commands. See detailed comparisons below:

|     | OS v3 and v4 | OSPi | OSBee |
|:----|:-------------------|:------------------------|:--------------------------|
|*Valve Compatibility*|**AC/DC** models both operate 24VAC valves. **DC** additionally supports DC non-latching valves. **Latch** supports only DC latching valves.|
|*Power Source*|**AC** works with a 24VAC adapter; **DC/Latch** work with a DC (6-24V) adapter.|24VAC only|USB|
|*Number of Stations*|`8` on main controller, expandable to `72` by linking expanders.|`8` on main controller, expandable to `200`.|`3`, not expandable.|
|*Processor*|**v3:** ESP8266<br>**v4:** ESP32-C6|RPi (user-supplied)|ESP8266|
|*Connectivity*|WiFi 2.4GHz (and optional wired Ethernet)|RPi's connectivity|WiFi 2.4GHz only|
|*Assembly*|Fully assembled commercial product in injection-molded enclosure|User provides RPi, SD card, and 3D printed enclosure|Fully assembled in 3D printed enclosure|
|*Weather*|Yes|Yes|No|
|*Physical Interface*|128×64 LCD, 3 buttons|128×64 LCD, 3 buttons|128×64 LCD, 1 button|
|*Target*|Everyone|RPi enthusiasts, tinkerers|Small garden projects|
|*Price*|$130~$160 USD|~$70 USD (plus the cost of RPi and accessories)|$62 USD|
|*Power Draw*|0.5-0.9 W|0.5 W + RPi's power|0.5 W|
|*Dimensions*|**v3.0-3.3**: 140×56×33 mm<br>**v3.4 and v4**: 125×79×25 mm|135×105×38 mm|65×65×20 mm|

---

**Q: How do I upgrade the firmware?**

* **OS v3 and v4:** Use the one-click **Update Firmware** feature from the controller's local web interface. Manual OTA firmware upload via WiFi or wired Ethernet is also supported.
* **OSPi:** Network or script-based firmware updates.

---

**Q: Will firmware update erase my settings?**

* **Dotted Version Updates**, e.g. `2.2.0` → `2.2.1`, automatically trigger a factory reset and erase all settings.
* **Build Number Updates**, e.g. `2.2.1(5)` → `2.2.1(6)`, normally preserve programs and settings. The 2.2.1(6) upgrade removes legacy sprinkler logs on v3, so download any logs you need before updating.

!!! info "Back Up Before Updating"
    Before any update, back up the current configurations so you can restore quickly if needed.

---

**Q: How can I set a static IP?**
<br>
The recommended way is to use your router's **DHCP Reservation** feature (sometimes called "Bind IP to MAC"). Alternatively, you can turn off the DHCP option on OpenSprinkler and manually set a static IP and gateway (router) IP.

---

**Q: How do I factory reset?**

* **OS v3 and v4:** Power cycle; when the OpenSprinkler logo appears, hold button `B1` until **Factory reset?** shows; confirm Yes, then hold `B3` until the controller restarts.
* **OSPi:** Stop the OpenSprinkler process; go to the folder where the firmware is installed, delete the file named `done.dat`; then restart the process.

---

**Q: How can I help with translations / multi-language support?**
<br>
OpenSprinkler's language localization is crowd-sourced; open the **About** page in the web UI to find the localization link.

---

**Q: Can I build my own app / Home Assistant integration?**
<br>
Absolutely. Please refer to the [OpenSprinkler Firmware API doc](api.md). Note that [Home Assistant integration](https://opensprinkler.com/forums/topic/home-assistant-integration/) for OpenSprinkler already exists, created by third-party developers.

---
