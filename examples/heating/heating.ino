
// Board: "Generic ESP8266 Module"
// Flash Size: "4MB (FS:2MB OTA:~1019KB)"
// Builtin Led: "2"

#include "app_config.h"
#include "build.gen.h"
#include "cmd_parse.h"

#include <SimpleNode.h>
#include <SimpleNodePages/config.h>
#include <SimpleNodePages/default.h>
#include <SimpleNodePages/update.h>

// ----------------------------------------------------------------------------

namespace app {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

node::Configuration< app::Config > configuration( CONFIG_FILENAME );
node::EmonCms emoncms;
node::Mqtt mqtt;
node::Ntp ntp;
node::Syslog syslog;
node::Uptime uptime;
node::Webserver web;
node::WiFi wifi;
node::WifiPatterns patterns( app::outputs::status_pin );

node::Logger app_log( "APP" );

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

  syslog.begin( SYSLOG_IP );
  syslog.set_level( node::Syslog::severity_info );
  node::Logger::use_syslog( syslog );

  uptime.begin();
  patterns.begin();

  wifi.begin();
  ntp.begin();

  mqtt.client_id( MQTT_CLIENT_ID );
  mqtt.pub_topic( MQTT_PUB_TOPIC );
  mqtt.sub_topic( MQTT_SUB_TOPIC, "cmd" );
  mqtt.begin( MQTT_HOST );

  emoncms.begin( EMONCMS_HOST, EMONCMS_API_KEY );

  webpages::register_default( web, uptime, app::build_version );
  webpages::register_config( web, (const uint8_t*)&configuration(), sizeof(app::Config) );
  webpages::register_update( web );
  web.begin();

  app_setup();
}

void loop( )
{
  static char buf[128];
  static char* ch = &buf[0];

  auto c = Serial.read();
  if (c > 0)
  {
    *ch++ = c;
    if (c == '\n')
    {
       *(ch - 1) = '\0';
       Serial.println(buf);
       parse_cmd(buf);
       ch = &buf[0];
    }
  }
}
