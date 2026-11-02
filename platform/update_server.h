#pragma once

#if defined(ESP8266)
#include "external/updater-esp8266/src/ESP8266WebServer.h"
#elif defined(ESP32)
#include "external/updater-esp32/src/WebServer.h"
#endif
