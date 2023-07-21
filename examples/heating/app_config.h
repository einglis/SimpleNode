#pragma once

// ----------------------------------------------------------------------------

// We want to ensure this is only built for the ESP8266 but, more importantly,
// only for the 4MB flash variant, to ensure we have space for OTA updates.

// Add this line:
//   compiler.cpp.extra_flags=-DESP8266_FLASH_BYTES={build.flash_size_bytes}
// to this file (or one very like it):
//   .arduino15/packages/esp8266/hardware/esp8266/3.0.2/platform.local.txt

#ifndef ESP8266_FLASH_BYTES
#error Check board settings; try "Generic ESP8266 Module"
#elif ESP8266_FLASH_BYTES != 0x400000
#error Check flash settings; try "4MB (FS:1MB OTA:~1019KB)"
#endif

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "heating"

#define MQTT_CLIENT "heating"
#define MQTT_SUB_TOPIC "heating/cmd"
#define MQTT_PUB_TOPIC "heating"

#define PATTERN_WIFI_GOT_IP   0x00000001

// ----------------------------------------------------------------------------

namespace app {

struct Config
{
  int counter;
};

namespace outputs {
  enum {
    status_pin = LED_BUILTIN, // nodeMCU D4 (aka 2)
    demand_hw_pin  = 14, // nodeMCU D5
    demand_ch1_pin = 12, // nodeMCU D6
    demand_ch2_pin = 13, // nodeMCU D7
    demand_ch3_pin = 15, // nodeMCU D8
  };
}
namespace inputs {
  enum {
    stat_hw_pin  = 16, // nodeMCU D0
    stat_ch1_pin =  5, // nodeMCU D1
    stat_ch2_pin =  4, // nodeMCU D2
  };
}

} // app