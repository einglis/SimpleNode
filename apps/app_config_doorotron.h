#pragma once

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "doorotron"

#define MQTT_CLIENT "doorotron"
#define MQTT_SUB_TOPIC "garage/cmd"
#define MQTT_PUB_TOPIC "garage/status"

// ----------------------------------------------------------------------------

namespace app {

namespace outputs {
  enum {
    status_pin = LED_BUILTIN, // nodeMCU D4 (aka 2)
    led_1_pin = 5, // nodeMCU D1
    led_2_pin = 4, // nodeMCU D2
    button_a_pin = 14, // nodeMCU D5
    button_b_pin = 13, // nodeMCU D7
  };
}
namespace inputs {
  enum {
    switch_a_pin = 16, // nodeMCU D0
    switch_b_pin = 12, // nodeMCU D6
  };
}

} // app
