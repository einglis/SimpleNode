#pragma once

// ----------------------------------------------------------------------------

// We want to ensure this is only built for the ESP8266 but, more importantly,
// only for the 1MB flash variant which is in the Sonoff Basics

// Add this line:
//   compiler.cpp.extra_flags=-DESP8266_FLASH_BYTES={build.flash_size_bytes}
// to this file (or one very like it):
//   .arduino15/packages/esp8266/hardware/esp8266/3.0.2/platform.local.txt

#ifndef ESP8266_FLASH_BYTES
#error Check board settings; try "Generic ESP8266 Module"
#elif ESP8266_FLASH_BYTES != 0x100000
#error Check flash settings; try "1MB (FS:64KB OTA:~470KB)"
#endif

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "sonoff"

#define MQTT_CLIENT "sonoff_client"
#define MQTT_SUB_TOPIC "sonoff/cmd"
#define MQTT_PUB_TOPIC "sonoff/status"

#define PATTERN_WIFI_GOT_IP   0xFFFFFFFF // no blinking

// ----------------------------------------------------------------------------

namespace app {

struct Config
{
  int counter;
};

namespace outputs {
  enum {
    led_pin_n = 13, // _n == active_low
    relay_pin = 12,
    status_pin = led_pin_n,
  };
}
namespace inputs {
  enum {
    button_pin_n = 0, // _n == active_low
  };
}

} // app
