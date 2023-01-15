#pragma once

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

#include "logging.h"
#include "wifi_observer.h"

namespace node {

class Mqtt
  : public node::WifiObserver
{
public:
  Mqtt()
    : client( &my_wifi, MQTT_HOST, MQTT_PORT, MQTT_CLIENT, ""/*key*/) // lazy config
    { }

  void setup( )
  {
    const bool will_rc = client.will( MQTT_PUB_TOPIC, "offline" );
    if (!will_rc)
      log.warning( F("failed to set will") );

    const bool keepalive_rc = client.setKeepAliveInterval( MQTT_KEEPALIVE * 1.5 );
    if (!keepalive_rc)
      log.warning( F("failed to set keepalive") );


    auto sub = std::make_shared<Adafruit_MQTT_Subscribe>( &client, MQTT_SUB_TOPIC );

    const bool sub_rc = client.subscribe( sub.get() );
    if (!sub_rc)
      log.warning( F("failed to subscribe to a topic") );
    else
      subs.push_back( sub );

    node::WifiObservers::add( this );
  }

  virtual void wifi_down( ) // WifiObserver
  {
    log.info( F("stopping") );
    client.disconnect();

    poll_ticker.detach();
    mqtt_disconnected();
  }

  virtual void wifi_up( ) // WifiObserver
  {
    log.info( F("starting") );
    poll();
  }

  typedef std::function< void(const char* cmd, const char* data) > handler_fn_t;
  void on( const char* stem, handler_fn_t fn )
  {
    auto handler = std::make_shared<MqttHandler>( stem, fn );
    handlers.push_back( handler );
  }

  bool publish( const char* payload )
  {
    return client.publish( MQTT_PUB_TOPIC, payload );
  }

private:
  WiFiClient my_wifi;
  Adafruit_MQTT_Client client;
  Ticker poll_ticker;
  Ticker ping_ticker;
  std::vector< std::shared_ptr<Adafruit_MQTT_Subscribe> > subs;

  struct MqttHandler
  {
    MqttHandler( const char* stem, handler_fn_t fn )
      : stem{ stem }
      , stem_len{ strlen(stem) }
      , fn{ fn }
      { }
    const char* stem;
    const size_t stem_len;
    const handler_fn_t fn;
  };
  std::vector<std::shared_ptr<MqttHandler>> handlers;

  void mqtt_connected( )
  {
    log.info( F("connected") );

    char buf[32]; // 16 bytes of ipv4 + some
    sprintf( buf, "online %s", wifi.ip().toString().c_str() );
    publish( buf );

    ping_ticker.attach_scheduled( MQTT_KEEPALIVE, [this]() {
      log.debug( F("ping") );
      client.ping(); // can block for up to 500 ms.
    } );
  }
  void mqtt_disconnected( )
  {
    log.info( F("disconnected") );
    ping_ticker.detach();
  }

  void handle_message( const char *msg, uint16_t )
  {
    for ( auto h : handlers )
      if (0 == strncmp(h->stem, msg, h->stem_len))
      {
        h->fn( msg, msg+h->stem_len );
        break;
      }
  }

  void poll( bool was_connected = false )
  {
    if (client.connected())
    {
      Adafruit_MQTT_Subscribe *sub = client.readSubscription( 0/*timeout*/ );
      for (int i = 0; sub && i < 10; i++) // i count is to limit potential batch size
      {
        handle_message((const char *)sub->lastread, sub->datalen);
        sub->new_message = false; // as per 'client.processSubscriptionPacket()'

        sub = client.readSubscription( 0/*timeout*/ );
      }
    }
    else // (!client.connected())
    {
      if (was_connected)
        mqtt_disconnected();

      log.info( F("trying to connect") );
      if (client.connect() == 0) /* 0 == connected*/
      {
        mqtt_connected();
        poll_ticker.attach_ms_scheduled( 100, [this](){ poll( true ); } );
      }
      else
      {
        // the first connection attempt invariably fails as the library
        // starts with a subcription sequence of zero, which is invalid.
        log.info( F("connect failed; will try again") );
        poll_ticker.once_scheduled( 5, [this](){ poll( false ); } );
      }
    }
  }

  static Logger log;
};

Logger Mqtt::log( "MQTT" );

} // node
