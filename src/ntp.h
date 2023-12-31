#pragma once

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <Ticker.h>
#include <WiFiUdp.h> // multiple options disambiguated by inclusion of ESP8266WiFi.h, somehow

#include "logging.h"
#include "wifi.h"

namespace node {

class Ntp
  : public node::WifiObserver
{
public:
  Ntp()
    : client( my_wifi, NTP_HOST/*lazy*/ )
  { }

  void begin( int report_interval = 0 )
  {
    if (report_interval)
      report_ticker.attach_scheduled( report_interval, [this](){
        log.infof( "time: %s", client.getFormattedTime() );
      } );

    WiFi::register_observer( *this );
  }

  bool epoch_valid( ) { return client.isTimeSet(); }
  long int epoch_time( ) { return client.getEpochTime(); }

  static int epoch_day( long int e )  { return (e / 86400L + 4) % 7; } // 0 == sunday
  static int epoch_hrs( long int e )  { return (e / 3600) % 24; }
  static int epoch_mins( long int e ) { return (e / 60) % 60; }
  static int epoch_secs( long int e ) { return  e % 60; }

  virtual void wifi_got_ip( IPAddress ) // WifiObserver
  {
    client.begin();
    refresh();
  }

  virtual void wifi_down() // WifiObserver
  {
    refresh_ticker.detach();
    client.end();
  }

private:
  WiFiUDP my_wifi;
  NTPClient client;
  Ticker report_ticker;
  Ticker refresh_ticker;

  void refresh( int phase = 0 )
  {
    log.debug( F("refresh") );

    int wait;
    if (client.update())
    {
      wait = 10 * 60; // ten minutes after success
      phase = 0; // restart at 1 second on next fail
    }
    else
    {
      wait = 1 << phase; // 1, 2, 4,... seconds
      phase = min( phase + 1, 5 ); // 1 << 5 == 32 seconds
    }

    refresh_ticker.once_scheduled( wait, [this, phase](){ refresh( phase ); } );
  }

  static Logger log;
};

} //node
