
// Board: "Generic ESP8266 Module"
// Flash Size: "4MB (FS:2MB OTA:~1019KB)"
// Builtin Led: "2"

#include "app_config.h"

#include "build.gen.h"
namespace app {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

#include <simple_node.h>

#include "pages/config.h"
#include "pages/default.h"
#include "pages/update.h"

#include "cmd_parse.h"

// ----------------------------------------------------------------------------

node::Configuration< app::Config > configuration( CONFIG_FILENAME );
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

  syslog.begin( SYSLOG_HOST );
  syslog.set_level( node::Syslog::severity_info );
  node::Logger::use_syslog( syslog );

  uptime.begin();
  patterns.begin();

  wifi.begin();
  ntp.begin();

  mqtt.client_id( MQTT_CLIENT );
  mqtt.pub_topic( MQTT_PUB_TOPIC );
  mqtt.begin( MQTT_HOST, MQTT_PORT );


  webpages::register_default( web, uptime );
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
