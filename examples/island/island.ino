
#include "app_config.h"

#include "build.gen.h"
namespace app {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

#include <simple_node.h>

#include "pages/config.h"
#include "pages/default.h"
#include "pages/update.h"

// ----------------------------------------------------------------------------

node::Configuration< app::Config > configuration( CONFIG_FILENAME );
node::Mqtt mqtt( MQTT_HOST, MQTT_CLIENT );
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
  Serial.println( app::build_version );
  Serial.println( ESP.getResetReason() );

  configuration.begin();

  uptime.begin();
  patterns.begin();
  pixels.begin();

  wifi.begin();
  ntp.begin();
  mqtt.begin();

  webpages::register_default( web, uptime );
  webpages::register_config( web, (const uint8_t*)&configuration(), sizeof(app::Config) );
  webpages::register_update( web );
  web.begin();

  app_setup();
}

void loop( )
{ }
