=========================================
=== OpenSprinklerGen2 Arduino Library ===
=========================================

The OpenSprinklerGen2 Library is an open-source, Arduino-based software library. The main component of the library is an OpenSprinklerGen2 class, which defines data structures and member functions to interface with OpenSprinkler hardware.

**********************************************************
Unless specified otherwise, all content is published under
the Creative Commons Attribution-ShareAlike 3.0 license.

Apr 2013 @ http://rayshobby.net
**********************************************************

The main files are:

* (defines.h): macro defines and Arduino pin assignemnts. 

* (OpenSprinklerGen2.h): class definition, including various data structures and member functions (such as functions to initialize hardware, set up options, process station operation, book keeping, rain delay, LCD, button etc.).

* (OpenSprinklerGen2.cpp): class implementation.

The library also depends on the following open-source Arduino libraries, including:

- Arduino LiquidCrystal, Time, DS1307, and Wire libraries
- JeeLabs's EtherCard library (http://jeelabs.net/projects/cafe/repository/show/EtherCard)
- Sanguino library


====== Notes ======

* (defines.h) contains a firmware (i.e. software) version number SVC_FW_VERSION. On start-up, this will be compared to the version number stored in OpenSprinkler internal EEPROM. If they are different, an automatic reset will be triggered. Since library update typically involves changing options (which are stored in EEPROM), the version number is typically incremented for each library update in order to trigger a reset and ensure the EEPROM data is consistent with the firmware.

* (defines.h) also contains a hardware version number SVC_HW_VERSION. This must be defined according to the hardware you have. A compilation error will be generated if this number is not defined. Since the library is designed to work with all hardware versions, and there is currently no way for the software to automatically figure that out, you must explicitly provide the hardware version. Your hardware version can be identified by the silkscreen number on the top of your controller circuit board. 

