
#include <Arduino.h>

#include "app_config.h"
#include "build.gen.h"

#include <SimpleNode.h>
using node::ButtonInput;

#include "SimpleNodePages/config.h"
#include "SimpleNodePages/default.h"
#include "SimpleNodePages/update.h"

// ----------------------------------------------------------------------------

namespace app {
const char *build_version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
};

node::Configuration< app::Config > configuration( CONFIG_FILENAME );
node::Mqtt mqtt;
node::Syslog syslog;
node::Uptime uptime;
node::Webserver web;
node::WiFi wifi;
node::WifiPatterns patterns( app::outputs::status_pin );

node::Logger app_log( "APP" );

// ----------------------------------------------------------------------------

bool power_state = false;

void report_status()
{
  mqtt.publish( "status", (power_state) ? "on" : "off" );
}

void set_power_state( bool new_state )
{
  power_state = new_state;
  digitalWrite( app::outputs::relay_pin, power_state );

  patterns.set_idle( power_state ? ~0 : 0 ); // make the LED match the relay
  report_status();
}

void toggle_power_state()
{
  set_power_state( !power_state );
}

// ----------------------------------------------------------------------------

ButtonInput button( [](){ return !digitalRead( app::inputs::button_pin_n ); } ); // active low

void button_event( ButtonInput::Event e, int count ) // called in SYS context
{
  // do this immediately for a timely response...
  if (e == ButtonInput::Press && count == 1)
    toggle_power_state();

  // ...reporting can be done in the next loop
  defer_to_loop( [=]() {

    const char* event_name = nullptr;
    switch (e)
    {
      case ButtonInput::Press:     event_name = "press"; break;
      case ButtonInput::HoldShort: event_name = "short"; break;
      case ButtonInput::HoldLong:  event_name = "long";  break;
      case ButtonInput::Final:     event_name = "final"; break;
      default: break; // unexpected
    }

    if (event_name)
    {
      char buf[32];
      sprintf( buf, "%s %d", event_name, count );
      mqtt.publish( "event", buf );
    }
  } );
}

// ----------------------------------------------------------------------------

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

  char mqtt_salt[9]; // distinguish mutliple sonoffs by MAC
  sprintf( mqtt_salt, "%04x", wifi.mac_ish() & 0xffff ); // two bytes is enough

  mqtt.client_id( MQTT_CLIENT_ID, mqtt_salt );
  mqtt.pub_topic( MQTT_PUB_TOPIC, mqtt_salt );
  mqtt.sub_topic( MQTT_SUB_TOPIC, "all", "cmd" );
  mqtt.sub_topic( MQTT_SUB_TOPIC, mqtt_salt, "cmd" );
  mqtt.begin( MQTT_HOST );

  webpages::register_default( web, uptime, app::build_version );
  webpages::register_config( web, (const uint8_t*)&configuration(), sizeof(app::Config) );
  webpages::register_update( web );
  web.begin();


  pinMode( app::inputs::button_pin_n, INPUT );
  button.begin( [](auto e, auto c){ button_event( e, c ); } );

  pinMode( app::outputs::led_pin_n, OUTPUT );
  pinMode( app::outputs::relay_pin, OUTPUT );
  set_power_state( false );

  mqtt.on( "on",     [](auto, auto){ set_power_state( true ); } );
  mqtt.on( "off",    [](auto, auto){ set_power_state( false ); } );
  mqtt.on( "toggle", [](auto, auto){ toggle_power_state(); } );
  mqtt.on( "status", [](auto, auto){ report_status(); } );
}

void loop( )
{ }
