
#include <Arduino.h>

#include "app_config.h"
#include "build.gen.h"

#include <SimpleNode.h>
#include "SimpleNodePages/default.h"
#include "SimpleNodePages/update.h"

// ----------------------------------------------------------------------------

namespace app {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

node::Mqtt mqtt;
node::Uptime uptime;
node::Webserver web;
node::WiFi wifi;
node::WifiPatterns patterns( app::outputs::status_pin );

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

  mqtt.client_id( MQTT_CLIENT_ID );
  mqtt.pub_topic( MQTT_PUB_TOPIC );
  mqtt.sub_topic( MQTT_SUB_TOPIC, "cmd" );
  mqtt.begin( MQTT_HOST );

  webpages::register_default( web, uptime, app::build_version );
  webpages::register_update( web );
  web.begin();

  pinMode( app::outputs::led_1_pin, OUTPUT );
  pinMode( app::outputs::led_2_pin, OUTPUT );

  a.begin();
  b.begin();

  mqtt.on( "status", [](auto, auto){ a.status(); b.status(); } );
}

void loop( )
{ }
