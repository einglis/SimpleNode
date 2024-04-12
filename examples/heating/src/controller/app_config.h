#pragma once

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "heating"

#define CONFIG_FILENAME "/heating_config.bin"

#define MQTT_CLIENT_ID "heating"
#define MQTT_SUB_TOPIC "heating"
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
    status_led = 15,
    demand_hw_led = 1,
    demand_ch1_led = 3,
    demand_ch2_led = 5,
    demand_ch3_led = 6,
    demand_hw_pin  = 37,
    demand_ch1_pin = 33,
    demand_ch2_pin = 18,
    demand_ch3_pin = 16,
  };
}
namespace inputs {
  enum {
    stat_hw_pin  = 21,
    stat_ch1_pin =  34,
    stat_ch2_pin =  38,
  };
}

} // app