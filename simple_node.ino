
#include "app_config.h"
#include "build.gen.h"
const char *Version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";

#include "node/inputs.h"
#include "node/logging.h"

// ----------------------------------------------------------------------------

#include "node/configuration.h"
node::Configuration< AppConfig > configuration( CONFIG_FILENAME );

#include "node/patterns.h"
node::Patterns patterns; // needs to precede anything that includes "node/wifi.h"

#include "node/mqtt.h"
node::Mqtt mqtt;

#include "node/ntp.h"
node::Ntp ntp;

#include "node/pixels.h"
node::Pixels pixels( NUM_PIXELS, node::outputs::pixels_pin );

#include "node/uptime.h"
node::Uptime uptime;

#include "node/webserver.h"
node::Webserver web;

#include "node/wifi.h" // requires node::Patterns patterns;
node::WiFi wifi;

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
  configuration.begin();
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
