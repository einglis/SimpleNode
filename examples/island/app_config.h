#pragma once

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "island"

#define MQTT_CLIENT "island_client"
#define MQTT_SUB_TOPIC "island/cmd"
#define MQTT_PUB_TOPIC "island/status"

#define PATTERN_WIFI_GOT_IP       0x00000000 // no blinking

// ----------------------------------------------------------------------------

namespace app {

namespace outputs {
  enum {
    status_pin = LED_BUILTIN, // aka 2
    pixels_pin = 14, // note: not the same as devboard
    power_pin  = 13,
  };
}

#define NUM_PIXELS 251
bool pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) > pixel_fn );

} // app
