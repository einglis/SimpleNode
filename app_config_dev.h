#pragma once

#define NODE_HAS_PIXELS  // ~3 kHz cost
#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_WEB_UPDATE
#define NODE_HAS_CONFIG

struct AppConfig
{
  int counter;
};

namespace node {
  namespace outputs {
    enum {
      status_pin = LED_BUILTIN, // 2 on devboard
      pixels_pin = 13,
      led_1_pin  = 14,
      led_2_pin  = 16,
    };
  }
  namespace inputs {
    enum {
      button_pin_n =  0, // _n == active low
      switch_pin_n = 12, // _n == active low
    };
  }
}

#define NUM_PIXELS 251 // deliberately stressful; dev actually only has 6
