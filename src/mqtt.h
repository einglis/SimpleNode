#pragma once

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

#include "logging.h"
#include "wifi.h"

namespace node {

class Mqtt
  : public node::WifiObserver
{
public:
  Mqtt( )
    : salty{ 0 }
    , client_id_buf{ 0 }
    , pub_topic_buf{ 0 }
    , pub_topic_len{ 0 }
    { }

  void salt( uint32_t s )
  {
    salty = s;
  }

  void client_id( const char* id )
  {
    const int salt_len = (salty) ? 9 : 0; // 9 for "_55AA1177"
    const size_t max_id_len = sizeof(client_id_buf) - salt_len - 1; // -1 for termination
    size_t id_len = std::min( strlen(id), max_id_len );
    memcpy( client_id_buf, id, id_len );
    if (salty)
      id_len += sprintf( client_id_buf + id_len, "_%08x", salty );
    client_id_buf[id_len] = '\0';
  }

  void pub_topic( const char* topic )
  {
    const int salt_len = (salty) ? 9 : 0; // 9 for "_55AA1177"
    const size_t max_topic_len = sizeof(pub_topic_buf) / 2 - salt_len - 1;
      // /2 to guarantee some space for further topic paths
      // -1 for termination
    pub_topic_len = std::min( strlen(topic), max_topic_len );
    memcpy( pub_topic_buf, topic, pub_topic_len);
    if (salty)
      pub_topic_len += sprintf( pub_topic_buf + pub_topic_len, "_%08x", salty );
    pub_topic_buf[pub_topic_len++] = '/';
    pub_topic_buf[pub_topic_len] = '\0';
  }

  void begin( const char* mqtt_host, int mqtt_port )
  {
    log.infof( "client id \"%s\"", client_id_buf );

    client = new Adafruit_MQTT_Client( &my_wifi, mqtt_host, mqtt_port, client_id_buf, ""/*user*/, ""/*password*/);
      // need to pass all params, otherwise we default-select the non-client version.

    const bool will_rc = client->will( make_pub_topic("connection"), "offline", MQTT_QOS_1/*at least once*/, 1/*retain*/ );
    if (!will_rc)
      log.warning( F("failed to set will") );

    const bool keepalive_rc = client->setKeepAliveInterval( MQTT_KEEPALIVE * 1.5 );
    if (!keepalive_rc)
      log.warning( F("failed to set keepalive") );


    auto sub = std::make_shared<Adafruit_MQTT_Subscribe>( client, MQTT_SUB_TOPIC );

    const bool sub_rc = client->subscribe( sub.get() );
    if (!sub_rc)
      log.warning( F("failed to subscribe to a topic") );
    else
      subs.push_back( sub );

    wifi_observer_register( *this );
  }

  virtual void wifi_down( ) // WifiObserver
  {
    log.info( F("stopping") );
    client->disconnect();

    poll_ticker.detach();
    mqtt_disconnected();
  }

  virtual void wifi_got_ip( IPAddress ) // WifiObserver
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

  // bool publish( const char* payload )
  // {
  //   return client->publish( pub_topic(""), payload );
  // }


private:
  WiFiClient my_wifi;
  uint32_t salty; // 0 == no salt
  char client_id_buf[32]; // needs to live as long as the client
  Adafruit_MQTT_Client *client;
  char pub_topic_buf[128];
  int pub_topic_len;
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

  const char* make_pub_topic( const char *tail )
  {
    const size_t max_tail_len = sizeof(pub_topic_buf) - pub_topic_len - 1; // -1 for termination
    const size_t tail_len = std::min( strlen(tail), max_tail_len );
    memcpy( pub_topic_buf + pub_topic_len, tail, tail_len);
    pub_topic_buf[pub_topic_len + tail_len] = '\0';
    return pub_topic_buf;
  }

  void mqtt_connected( )
  {
    log.info( F("connected") );

    char buf[32]; // 16 bytes of ipv4 + some
    sprintf( buf, "online %s", my_wifi.localIP().toString().c_str() );
    (void)client->publish( make_pub_topic("connection"), buf, MQTT_QOS_1/*at least once*/, 1/*retain*/ );

    ping_ticker.attach_scheduled( MQTT_KEEPALIVE, [this]() {
      log.debug( F("ping") );
      client->ping(); // can block for up to 500 ms.
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
        const char *data = msg + h->stem_len;
        while (data && *data == ' ')
          data++; // skip spaces

        h->fn( msg, data );
        break;
      }
  }

  void poll( bool was_connected = false )
  {
    if (client->connected())
    {
      Adafruit_MQTT_Subscribe *sub = client->readSubscription( 0/*timeout*/ );
      for (int i = 0; sub && i < 10; i++) // i count is to limit potential batch size
      {
        handle_message((const char *)sub->lastread, sub->datalen);
        sub->new_message = false; // as per 'client->processSubscriptionPacket()'

        sub = client->readSubscription( 0/*timeout*/ );
      }
    }
    else // (!client->connected())
    {
      if (was_connected)
        mqtt_disconnected();

      log.info( F("trying to connect") );
      if (client->connect() == 0) /* 0 == connected*/
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
