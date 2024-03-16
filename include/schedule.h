
#pragma once

// This is a wrapper, mostly to hide differences and deficiencies in the ESP8266 and ESP32 Tickers
// I _could_ re-implement their innards, but this is the most expedient solution at present.

#if ESP32
#include <Ticker.h>
#include "esp_timer.h"
#elif ESP8266
#include <Ticker.h>
#else
#error unsupported architecture
#endif

#include <variant>


void defer_to_loop(const std::function<void(void)>& fn);


namespace node {

class Ticker
{
public:
  typedef std::function<void()> callback_function_t;
  void once( uint32_t delay_ms, callback_function_t fn )
  {
    detach();
    callback_fn = std::move( fn );

    #if ESP32
    t.once_ms( delay_ms, trampoline, reinterpret_cast< void* >( this ) );
    #elif ESP8266
    //t.once_ms_scheduled( delay_ms, trampoline, reinterpret_cast< void* >( this ) );
    t.once_ms_scheduled( delay_ms, fn );
    #endif
}

  void repeat( uint32_t interval_ms, callback_function_t fn )
  {
    detach();
    callback_fn = std::move( fn );

    // _Always_ called the scheduled variant (aka executed in the loop rather than an interrupt)
    // for ESP32, that's the one and only option with the standard interface
    // for ESP8266, it's a different variant of the function; the default is in SYS (aka interrupt) context

    #if ESP32
    t.attach_ms( interval_ms, trampoline, reinterpret_cast< void* >( this ) );
    #elif ESP8266
    //t.attach_ms_scheduled( interval_ms, trampoline, reinterpret_cast< void* >( this ) );
    t.attach_ms_scheduled( interval_ms, fn );
    #endif
  }

  void detach( )
  {
    t.detach();
    callback_fn = std::monostate{};
  }

private:
  ::Ticker t;
  std::variant< std::monostate, callback_function_t > callback_fn;

  static void trampoline( void* context )
  {
    Ticker* t = reinterpret_cast< Ticker* >( context );
    if (auto callback_fn = std::get_if< callback_function_t >( &t->callback_fn ))
      (*callback_fn)();
  }
};

// inline void ticker_repeat( Ticker& t, uint32_t ms, std::function<void()> fn )
// {
//   #if ESP32
//   t.attach_ms( ms, fn );
//   #elif ESP8266
//   t.attach_ms_scheduled( ms, fn );
//   #endif
// }

// inline void ticker_once( Ticker& t, uint32_t ms, callback_function_t fn )
// {
//   #if ESP32
//   t.once_ms( ms, fn );
//   #elif ESP8266
//   t.once_ms_scheduled( ms, fn );
//   #endif
// }



class IsrTicker
{
public:
  typedef std::function<void()> callback_function_t;
  IsrTicker( uint32_t ms, callback_function_t fn )
    : ms{ ms }
    , callback_fn{ std::move(fn) }
    #if ESP32
    , timer{ nullptr }
    #endif
  {  }

  void begin( )
  {
    #if ESP32
    // borrowed from framework-arduinoespressif32/libraries/Ticker/src/Ticker.cpp
    // which is Copyright (c) 2017 Bert Melis.

    esp_timer_create_args_t timer_config;
    timer_config.arg = reinterpret_cast< void* >( this );
    timer_config.callback = trampoline;
    timer_config.dispatch_method = ESP_TIMER_ISR;
    timer_config.name = "IsrTicker";
    esp_timer_create( &timer_config, &timer );
    esp_timer_start_periodic( timer, ms * 1000ULL);

    #elif ESP8266
    ticker.attach_ms( ms, callback_fn );

    #endif
  }

private:
  #if ESP32
  esp_timer_handle_t timer;
  #elif ESP8266
  Ticker ticker;
  #endif
  const uint32_t ms;
  const callback_function_t callback_fn;

  #if ESP32
  static void trampoline( void* context )
  {
    IsrTicker* t = reinterpret_cast< IsrTicker* >( context );
    t->callback_fn();
  }
  #endif

};

} // node
