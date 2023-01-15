#pragma once

#include <Ticker.h>

namespace node {

class Patterns
{
public:
  Patterns( int pin = PATTERN_PIN )
    : pin{ pin }
    , pattern{ 0 }
  { }

  void setup()
  {
    pinMode( pin, OUTPUT );

    ticker.attach_ms( 100, [this]() {
      pattern = (pattern >> 1) | (pattern << 31); // roll right 1
      digitalWrite( pin, ~(pattern) & 1 ); // active low
    } );
  }

  void set( uint32_t p )
  {
    pattern = p;
  }

private:
  const int pin;
  uint32_t pattern;
  Ticker ticker;
};

} // node
