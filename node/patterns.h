#pragma once

#include <Ticker.h>

#include "wifi.h" // this is a bit ugly in a class that is fundamentally distinct from wifi

namespace node {

class Patterns
{
public:
  Patterns( int pin )
    : pin{ pin }
    , pattern{ 0 }
  { }

  void begin()
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

// ------------------------------------

struct WifiPatterns
  : public Patterns
  , public WifiObserver
{
  WifiPatterns( int pin )
    : Patterns( pin )
  {
    wifi_down();
  }

  virtual void wifi_down( ) { set( PATTERN_WIFI_DISCONNECTED ); };
  virtual void wifi_up( ) { set( PATTERN_WIFI_CONNECTED ); };
  virtual void wifi_got_ip( IPAddress ) { set( PATTERN_WIFI_GOT_IP ); };

  void begin( )
  {
    Patterns::begin();
    wifi_observer_register( *this );
  }
};

} // node
