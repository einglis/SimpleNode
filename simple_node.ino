
#include "app_config.h"
#include "build.gen.h"
const char *Version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";

#include "node/defaults.h" // needs to come first for now
#include "node/configuration.h"
#include "node/inputs.h"
#include "node/logging.h"
#include "node/mqtt.h" // costs about 9k loops/sec
#include "node/ntp.h"
#include "node/patterns.h"
#include "node/pixels.h" // costs about 3k loops/sec
#include "node/uptime.h"
#include "node/webserver.h"
#include "node/wifi.h"

// ----------------------------------------------------------------------------

node::Configuration< app::Config > configuration( CONFIG_FILENAME );
node::Mqtt mqtt;
node::Ntp ntp;
node::WifiPatterns patterns( app::outputs::status_pin );
node::Pixels pixels( app::outputs::pixels_pin, NUM_PIXELS, app::pixels_update );
node::Uptime uptime;
node::Webserver web;
node::WiFi wifi;

// ------------------------------------

void setup( )
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println( Version );
  Serial.println( ESP.getResetReason() );

  configuration.begin();

  uptime.begin();
  patterns.begin();
  pixels.begin();

  wifi.begin();
  ntp.begin( 11 /*report interval in seconds*/ );
  mqtt.begin( );
  web.begin();
  register_web_pages( web ); // move to app_setup?

  app_setup();
}

void loop( )
{
  static node::Logger log( "LOOP" );
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
