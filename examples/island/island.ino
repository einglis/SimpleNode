
#include "app_config.h"

#include "build.gen.h"
namespace app {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

#include <simple_node.h>

#include "pages/default.h"
#include "pages/update.h"

// ----------------------------------------------------------------------------

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
  Serial.println( app::build_version );
  Serial.println( ESP.getResetReason() );

  uptime.begin();
  patterns.begin();
  pixels.begin();

  wifi.begin();
  ntp.begin();
  mqtt.begin();

  webpages::register_default( web, uptime );
  webpages::register_update( web );
  web.begin();

  app_setup();
}

void loop( )
{ }
