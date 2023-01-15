
#include <Adafruit_NeoPixel.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <Ticker.h>
#include <WiFiUdp.h>

#include "app_config.h"
#include "build.gen.h"
const char *Version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
#include "logging.h"

// ----------------------------------------------------------------------------

#include "node/configuration.h"
node::Configuration configuration;

#include "node/patterns.h"
node::Patterns patterns;

#include "node/uptime.h"
node::Uptime uptime;

#include "node/wifi_observer.h"

#include "node/webserver.h"
node::Webserver web;

#include "node/wifi.h"
node::WiFi wifi;

#include "node/mqtt.h" // requires wifi
node::Mqtt mqtt;

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_NTP

class Ntp
  : public node::WifiObserver
{
public:
  Ntp()
    : client( my_wifi, NTP_HOST/*lazy*/ )
  { }

  void setup( int report_interval = 0 )
  {
    if (report_interval)
      report_ticker.attach_scheduled( report_interval, [this](){
        log.infof( "time: %s", client.getFormattedTime() );
      } );

    node::WifiObservers::add( this );
  }

  long int epoch_time( ) { return client.getEpochTime(); }

  static int epoch_day( long int e )  { return (e / 86400L + 4) % 7; } // 0 == sunday
  static int epoch_hrs( long int e )  { return (e / 3600) % 24; }
  static int epoch_mins( long int e ) { return (e / 60) % 60; }
  static int epoch_secs( long int e ) { return  e % 60; }

  virtual void wifi_up() // WifiObserver
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

Ntp ntp;
Logger Ntp::log( "NTP" );

#endif

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_PIXELS

class Pixels
{
public:
  Pixels( int num_pixels = NUMPIXELS, int pin = PIXELS_PIN ) // lazy
    : pixels( num_pixels, pin, NEO_GRB + NEO_KHZ800 )
    , work_phase{ 0 }
    , need_update{ false }
    { }

  void setup()
  {
    //pinMode( pin, OUTPUT );
      // the Adafruit_NeoPixel constructor will have done this for us

    pixels.begin(); // This initializes the NeoPixel library.

    ticker.attach_ms( 50, [this](){ update(); } );
  }

  void brightness( uint8_t b )
  {
    pixels.setBrightness( b );
  }

private:
  Adafruit_NeoPixel pixels;
  Ticker ticker;
  int work_phase;
  bool need_update;

  void update( )
  {
    // called in SYS context, so be quick

    if (work_phase == 0)
    {
      need_update = app_pixels_update( pixels.numPixels(),
        [this](uint16_t n, uint32_t c){ pixels.setPixelColor( n, c ); } );
    }
    else
    {
      if (need_update)
        pixels.show();
    }

    work_phase = !work_phase;
  }

};

Pixels pixels;

#endif

// ----------------------------------------------------------------------------

void setup( )
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println( Version );
  Serial.println( ESP.getResetReason() );

  uptime.setup();
  patterns.setup();

#ifdef NODE_HAS_CONFIG
  configuration.setup();
#endif

  wifi.setup();

#ifdef NODE_HAS_NTP
  ntp.setup( 11 /*report interval in seconds*/ );
#endif
#ifdef NODE_HAS_MQTT
  mqtt.setup( );
#endif
#ifdef NODE_HAS_PIXELS
  pixels.setup();
#endif
#ifdef NODE_HAS_WEB
  web.setup();
  register_web_pages( web );
#endif

  app_setup();
}

void loop( )
{
  static Logger log( "LOOP" );
  static long last = millis();
  static int loops = 0;

  loops++;
  long now = millis();
  if (now - last > 7000) // magic, but arbitrary number
  {
    log.debugf( "loop rate is about %d Hz", loops * 1000 / (now-last) );
    loops = 0;
    last = now;
  }
}
