#pragma once

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "sonoff"

#define MQTT_CLIENT_ID "sonoff" // +salt
#define MQTT_PUB_TOPIC "sonoff" // +salt
#define MQTT_SUB_TOPIC "sonoff" // +salt

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
