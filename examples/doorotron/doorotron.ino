
#include "app_config.h"

#include "build.gen.h"
namespace app {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

#include <simple_node.h>

#include "pages/default.h"
#include "pages/update.h"

// ----------------------------------------------------------------------------

node::Mqtt mqtt( MQTT_HOST, MQTT_CLIENT );
node::WifiPatterns patterns( app::outputs::status_pin );
node::Uptime uptime;
node::Webserver web;
node::WiFi wifi;

// ------------------------------------

node::Logger app_log( "APP" );

#include "doors.h"

Door a( "doorA", app::inputs::switch_a_pin, app::outputs::button_a_pin );
Door b( "doorB", app::inputs::switch_b_pin, app::outputs::button_b_pin );

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

  wifi.begin();
  mqtt.begin( );

  webpages::register_default( web, uptime );
  webpages::register_update( web );
  web.begin();

  pinMode( app::outputs::led_1_pin, OUTPUT );
  pinMode( app::outputs::led_2_pin, OUTPUT );

  a.begin();
  b.begin();
}

void loop( )
{ }
