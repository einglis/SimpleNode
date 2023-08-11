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

#define WIFI_HOSTNAME "island"

#define MQTT_CLIENT "island"
#define MQTT_SUB_TOPIC "island"
#define MQTT_PUB_TOPIC "island"

#define PATTERN_WIFI_GOT_IP       0x00000000 // no blinking

#define CONFIG_FILENAME "/island_config.bin"

// ----------------------------------------------------------------------------

namespace app {

struct Config
{
  union {
    struct {
      int counter;
      int power;
      int pattern;
    };
    uint8_t pad[64];
  };
};

namespace outputs {
  enum {
    status_pin = LED_BUILTIN, // aka 2
    pixels_pin = 14,
    power_pin  = 13,
  };
}

#define NUM_PIXELS 251
bool pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) > pixel_fn );

} // app
