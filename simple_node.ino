
#include "app_config.h"
#include "build.gen.h"
const char *Version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";

#include "node/configuration.h"
#include "node/inputs.h"
#include "node/logging.h"
#include "node/mqtt.h"
#include "node/ntp.h"
#include "node/patterns.h"
#include "node/pixels.h"
#include "node/uptime.h"
#include "node/webserver.h"
#include "node/wifi.h"

// ----------------------------------------------------------------------------

node::Configuration< AppConfig > configuration( CONFIG_FILENAME );
node::Mqtt mqtt;
node::Ntp ntp;
node::WifiPatterns patterns( node::outputs::status_pin );
node::Pixels pixels( NUM_PIXELS, node::outputs::pixels_pin );
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
