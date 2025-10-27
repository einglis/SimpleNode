#pragma once

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "lorarelay"

#define MQTT_CLIENT_ID "lorarelay"
#define MQTT_SUB_TOPIC "lorarelay"
#define MQTT_PUB_TOPIC "lorarelay"

// ----------------------------------------------------------------------------

namespace app {

namespace outputs {
  enum {
    status_pin = LED_BUILTIN, // nodeMCU D4 (aka 2)
    swSerialTxPin = 5, // nodeMCU D1
  };
}
namespace inputs {
  enum {
    swSerialRxPin = 4, // nodeMCU D2
  };
}

} // app
