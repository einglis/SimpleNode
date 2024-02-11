#pragma once

#include <ESP8266WiFi.h>

#include "logging.h"
#include "schedule.h"

namespace node {

// ------------------------------------

struct WifiObserver
{
  virtual void wifi_got_ip( IPAddress ) { };
  virtual void wifi_up() { };
  virtual void wifi_down() { };

  virtual ~WifiObserver() = 0;
};

// ------------------------------------

class WiFi
{
public:
  WiFi( )
    : is_connected{ false }
    { }

  void begin()
  {
    ::WiFi.macAddress( mac );
    log.infof( "MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    ::WiFi.hostname( WIFI_HOSTNAME );
    ::WiFi.persistent(false); // don't stash config in Flash

    handlers.push_back( ::WiFi.onStationModeDisconnected( [this](auto e){ wifi_disconnected(e); } ) );
    handlers.push_back( ::WiFi.onStationModeConnected( [this](auto e){ wifi_connected(e); } ) );
    handlers.push_back( ::WiFi.onStationModeGotIP( [this](auto e){ wifi_got_ip(e); } ) );

    log.info( F("inital connection...") );

    ::WiFi.mode( WIFI_STA );
    ::WiFi.begin( WIFI_SSID, WIFI_PASSWD );
      // it transpires that this will keep looking forever,
      // _and_ try to reconnect if the connection is lost.
  }

  IPAddress ip() const { return my_ip; }
  uint32_t mac_ish() const { return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | (mac[5] << 0); }

private:
  std::vector< WiFiEventHandler > handlers;
  bool is_connected;
  IPAddress my_ip;

  void wifi_disconnected( const WiFiEventStationModeDisconnected& )
  {
    if (!is_connected) // this function tends to get called over-and-over
      return

    log.info( F("disconnected") );
    is_connected = false;
    my_ip.clear();

    defer_to_loop( []() { // decouple schedulling, just in case.
      for ( WifiObserver& o : observers )
        o.wifi_down();
    } );
  }

  void wifi_connected( const WiFiEventStationModeConnected & )
  {
    log.info( F("connected") );
    is_connected = true;

    defer_to_loop( []() { // decouple schedulling, just in case.
      for ( WifiObserver& o : observers )
        o.wifi_up();
    } );
  }

  void wifi_got_ip( const WiFiEventStationModeGotIP &e )
  {
    log.infof( "got IP: %s", e.ip.toString().c_str() );
    my_ip = e.ip;

    defer_to_loop( [e]() { // decouple schedulling, just in case.
      for ( WifiObserver& o : observers )
        o.wifi_got_ip( e.ip );
    } );
  }

  uint8_t mac[8];

  static Logger log;

  static std::vector< std::reference_wrapper< WifiObserver > > observers;
public:
  static void register_observer( WifiObserver& o) { observers.push_back( o ); }
};

} // node
