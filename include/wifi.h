#pragma once

#if ESP8266
//#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#elif ESP32
//#include <HTTPClient.h>
#include <WiFi.h>
#endif

#include "events.h"
#include "logging.h"
#include "schedule.h"

namespace node {

// ------------------------------------

struct WifiObserver
{
  virtual void wifi_got_ip( ) { };
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
    , my_ip{ INADDR_NONE }
    , events{ 4 } // seems more than enough
    { }

  const int connect_check_ms = 20 * 1000;
  const int reconnect_holdoff_ms = 3 * 1000;

  void begin()
  {
    ::WiFi.macAddress( mac );
    //log.infof( "MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    ::WiFi.hostname( WIFI_HOSTNAME );
    ::WiFi.persistent(false); // don't stash config in Flash

    #if ESP8266
    (void) ::WiFi.onStationModeDisconnected( [this](auto){ wifi_disconnected(); } );
    (void) ::WiFi.onStationModeConnected( [this](auto){ wifi_connected(); } );
    (void) ::WiFi.onStationModeGotIP( [this](auto e){ wifi_got_ip(e.ip); } );

    #elif ESP32
    (void) ::WiFi.onEvent( [this](arduino_event_id_t, arduino_event_info_t){ wifi_disconnected(); }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    (void) ::WiFi.onEvent( [this](arduino_event_id_t, arduino_event_info_t){ wifi_connected(); }, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    (void) ::WiFi.onEvent( [this](arduino_event_id_t, arduino_event_info_t i){ wifi_got_ip(IPAddress{i.got_ip.ip_info.ip.addr}); }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

    #else
    #error unsupported architecture
    #endif

    event_ticker.repeat( 100, [this]() { event_ticker_fn(); } );

    log.info( F("inital connection...") );
    ::WiFi.mode( WIFI_STA );
    ::WiFi.begin( WIFI_SSID, WIFI_PASSWD );
      // it transpires that this will keep looking forever,
      // _and_ try to reconnect if the connection is lost.

    reconnect_ticker.once( connect_check_ms, [this]() { check_connection(); } );
  }

  IPAddress ip() const { return my_ip; }
  uint32_t mac_ish() const { return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | (mac[5] << 0); }

private:
  bool is_connected;
  IPAddress my_ip;

  enum event_t { WIFI_none, WIFI_disconnected, WIFI_connected, WIFI_got_ip };
  node::Events< event_t > events;
  node::Ticker event_ticker;
  node::Ticker reconnect_ticker;

  void wifi_disconnected( )
  {
    if (!is_connected) // this function tends to get called over-and-over
      return;

    log.info( F("disconnected") );
    is_connected = false;
    my_ip = INADDR_NONE;

    events.add( event_t{ WIFI_disconnected } );

//    defer_to_loop( []() { // decouple schedulling, just in case.
//      for ( WifiObserver& o : observers )
//        o.wifi_down();
//    } );
  }

  void wifi_connected( )
  {
    log.info( F("connected") );
    is_connected = true;

    events.add( event_t{ WIFI_connected } );

//    defer_to_loop( []() { // decouple schedulling, just in case.
//      for ( WifiObserver& o : observers )
//        o.wifi_up();
//    } );
  }

  void wifi_got_ip( IPAddress ip )
  {
    log.infof( "got IP: %s", ip.toString().c_str() );
    my_ip = ip;

    events.add( event_t{ WIFI_got_ip } );

//    defer_to_loop( [ip]() { // decouple schedulling, just in case.
//      for ( WifiObserver& o : observers )
//        o.wifi_got_ip( );
//    } );
  }

  void reconnect( )
  {
    log.info( F("reconnecting") );
    reconnect_ticker.once( connect_check_ms, [this]() { check_connection(); } );
    ::WiFi.reconnect();
  }

  void check_connection( )
  {
      if (!is_connected || my_ip == INADDR_NONE)
      {
        log.warning( F("not connected or no IP; reconnecting") );
        reconnect( );
      }
  }

  void event_ticker_fn( )
  {
    event_t event = WIFI_none;
    bool work = events.remove( event );
    if (!work)
      return;

    switch( event )
    {
      case WIFI_disconnected:
        log.info( F("disconnected callback") );
        for ( WifiObserver& o : observers )
          o.wifi_down();

        reconnect_ticker.once( reconnect_holdoff_ms, [this]() { reconnect(); } );
        break;

      case WIFI_connected:
        log.info( F("connected callback") );
        for ( WifiObserver& o : observers )
          o.wifi_up();
        break;

      case WIFI_got_ip:
        log.info( F("got ip callback") );
        reconnect_ticker.detach();

        for ( WifiObserver& o : observers )
          o.wifi_got_ip();
        break;

      default:
        log.infof( "got another callback! %d", (int)event );
        break;
    }
}

  uint8_t mac[8];

  static Logger log;

  static std::vector< std::reference_wrapper< WifiObserver > > observers;
public:
  static void register_observer( WifiObserver& o) { observers.push_back( o ); }
};

} // node
