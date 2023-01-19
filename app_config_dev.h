#pragma once

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
bool app_pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) > pixel_fn );
