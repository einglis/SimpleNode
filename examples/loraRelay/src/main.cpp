
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

void modem_begin();
void modem_loop();

// ------------------------------------

node::Logger app_log( "APP" );

void setup( )
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println( app::build_version );
  //Serial.println( ESP.getResetReason() ); // ESP8266 (string)
  Serial.println( esp_reset_reason() ); // ESP32 (enum)

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

  modem_begin();
}

void loop( )
{
  modem_loop();
}
