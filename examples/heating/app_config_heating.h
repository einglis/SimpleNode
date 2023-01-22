#pragma once

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "heat"

#define MQTT_CLIENT "heat_client"
#define MQTT_SUB_TOPIC "heat/cmd"
#define MQTT_PUB_TOPIC "heat/status"

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