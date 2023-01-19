#pragma once

#include <ESP8266WiFi.h>

#include "logging.h"

namespace node {

Logger wifi_log( "WIFI" );

// ------------------------------------

struct WifiObserver
{
  virtual void wifi_got_ip( IPAddress ) { };
  virtual void wifi_up() { };
  virtual void wifi_down() { };

  void wifi_observer_register( WifiObserver& );
  virtual ~WifiObserver() = 0;
};

WifiObserver::~WifiObserver( ) { }
  // pure virtual destructor implementation

// ------------------------------------

class WiFi
{
public:
  WiFi( )
    : is_connected{ false }
    , log{ wifi_log }
    { }

  void setup()
  {
    log.infof( "MAC: %s", ::WiFi.macAddress().c_str() );

    ::WiFi.hostname( WIFI_HOSTNAME );
    ::WiFi.persistent(false); // don't stash config in Flash

    handlers.push_back( ::WiFi.onStationModeDisconnected( [this](auto e){ wifi_disconnected(e); } ) );
    handlers.push_back( ::WiFi.onStationModeConnected( [this](auto e){ wifi_connected(e); } ) );
    handlers.push_back( ::WiFi.onStationModeGotIP( [this](auto e){ wifi_got_ip(e); } ) );

    log.info( F("inital connection...") );
    patterns.set( PATTERN_WIFI_DISCONNECTED );

    ::WiFi.mode( WIFI_STA );
    ::WiFi.begin( WIFI_SSID, WIFI_PASSWD );
      // it transpires that this will keep looking forever,
      // _and_ try to reconnect if the connection is lost.
  }

  IPAddress ip() { return my_ip; }

private:
  std::vector< WiFiEventHandler > handlers;
  bool is_connected;
  IPAddress my_ip;

  void wifi_disconnected( const WiFiEventStationModeDisconnected& )
  {
    if (!is_connected) // this function tends to get called over-and-over
      return

    patterns.set( PATTERN_WIFI_DISCONNECTED );
    log.info( F("disconnected") );
    is_connected = false;
    my_ip.clear();

    schedule_function( []() { // decouple schedulling, just in case.
      for ( WifiObserver& o : observers )
        o.wifi_down();
    } );
  }

  void wifi_connected( const WiFiEventStationModeConnected & )
  {
    patterns.set( PATTERN_WIFI_CONNECTED );
    log.info( F("connected") );
    is_connected = true;

    schedule_function( []() { // decouple schedulling, just in case.
      for ( WifiObserver& o : observers )
        o.wifi_up();
    } );
  }

  void wifi_got_ip( const WiFiEventStationModeGotIP &e )
  {
    patterns.set( PATTERN_WIFI_GOT_IP );
    log.infof( "got IP: %s", e.ip.toString().c_str() );
    my_ip = e.ip;

    schedule_function( [e]() { // decouple schedulling, just in case.
      for ( WifiObserver& o : observers )
        o.wifi_got_ip( e.ip );
    } );
  }

  Logger& log;

  static std::vector< std::reference_wrapper< WifiObserver > > observers; // declaration...
  friend void WifiObserver::wifi_observer_register( WifiObserver& );
};

// ------------------------------------

std::vector< std::reference_wrapper< WifiObserver > > WiFi::observers; // ...definition
void WifiObserver::wifi_observer_register( WifiObserver& o )
{
  WiFi::observers.push_back( o );
}

} // node
