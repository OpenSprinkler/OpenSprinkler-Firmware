============================================
==== OpenSprinkler AVR/RPI/BBB Firmware ====
============================================

This is a unified OpenSprinkler firmware for
Arduino, RPi, BBB based OpenSprinklers.

--------------------------------------------
For microcontroller (Arduino):

Option 1: install Arduino make, then open
Makefile and modify the path to your Arduino
installation folder accordingly. Next, run
> make
in command line.

Option 2: install Arduino (1.0.6 recommended)
and copy the OpenSprinkler folder to your
Arduino's libraries folder. 

--------------------------------------------
For RPi:

Run the following command:
gcc -o opensprinkler -DOSPI -m32 main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp

--------------------------------------------

For BBB:

Run the following command:
gcc -o opensprinkler -DOSBO -m32 main.cpp OpenSprinkler.cpp program.cpp server.cpp utils.cpp weather.cpp

============================================
Questions and comments:
http://www.opensprinkler.com
============================================
