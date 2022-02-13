#!/usr/bin/env python3
#====================================================================================
# py_wrap.pl
#
# Wrapper for python scripts in the esp8266 Arduino tools directory
#
# This file is part of makeESPArduino
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/makeEspArduino
#
# Copyright (c) 2021 Peter Lerup. All rights reserved.
#
#====================================================================================

import sys
import os

sys.argv.pop(0)
root_dir = sys.argv.pop(0)
is_module = sys.argv[0] == "-m"
if is_module:
    sys.argv.pop(0)
script = sys.argv[0]
# Include the required module directories to the path
sys.path.insert(0, root_dir + "/pyserial")
sys.path.insert(0, root_dir + "/esptool")
exec("import " + script)
if not is_module:
    sys.argv.pop(0)
exec(script + ".main(sys.argv)")
