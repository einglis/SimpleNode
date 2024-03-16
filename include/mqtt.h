#pragma once

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Ticker.h>

#include "logging.h"
#include "wifi.h"

static void my_strncpy( char* &dst, const char* src, int &n ) // ugly :(
{
  while (dst && src && *src && n > 0)
  {
    *dst++ = *src++;
    n--;
  }
}

namespace node {

#define DEFAULT_MQTT_PORT 1883

#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE  60  // timeout is set to 1.5x this value
#endif

// ----------------------------------------------------------------------------

class Mqtt
  : public node::WifiObserver
{
public:
  Mqtt( )
    : client_id_buf{ 0 }
    , pub_topic_buf{ 0 }
    , pub_topic_len{ 0 }
    { }

  void client_id( const char* base, const char* salt = nullptr )
  {
    char* bp = &client_id_buf[0];
    int buf_left = sizeof(client_id_buf) - 1; // -1 for termination

    my_strncpy( bp, base, buf_left );
    if (salt && salt[0])
    {
      my_strncpy( bp, "_",  buf_left ); // concatenator
      my_strncpy( bp, salt, buf_left );
    }
    *bp = '\0'; // my_strncpy does not terminate.
  }

  void pub_topic( const char* base, const char* salt = nullptr )
  {
    char* bp = &pub_topic_buf[0];
    int buf_left = sizeof(pub_topic_buf) - 1; // -1 for termination

    my_strncpy( bp, base, buf_left );
    if (salt && salt[0])
    {
      my_strncpy( bp, "/",  buf_left ); // separator
      my_strncpy( bp, salt, buf_left );
    }
    *bp = '\0';

    pub_topic_len = strlen(pub_topic_buf); // could calculate, but this seems cleaner somehow.
  }

  void sub_topic( const char* base, const char* salt, const char* tail )
  {
    auto sub = std::make_shared<MqttSubscription>( );

    // do this here, rather than in the MqttSubscription, just to keep similar operations near one another
    char* bp = &sub->sub_topic_buf[0];
    int buf_left = sizeof(sub->sub_topic_buf) - 1; // -1 for termination

    my_strncpy( bp, base, buf_left );
    if (salt && salt[0])
    {
      my_strncpy( bp, "/",  buf_left ); // separator
      my_strncpy( bp, salt, buf_left );
    }
    my_strncpy( bp, "/",  buf_left ); // separator
    my_strncpy( bp, tail, buf_left );
    *bp = '\0';

    subscriptions.push_back( sub );
  }

  void sub_topic( const char* base, const char* tail )
  {
    sub_topic( base, nullptr, tail );
  }


  void begin( const char* mqtt_host, int mqtt_port = DEFAULT_MQTT_PORT )
  {
    log.infof( "client id \"%s\"", client_id_buf );

    client = new Adafruit_MQTT_Client( &my_wifi, mqtt_host, mqtt_port, client_id_buf, ""/*user*/, ""/*password*/);
      // need to pass all params, otherwise we default-select the non-client version.

    const bool will_rc = client->will( make_will_topic("connection"), "offline", MQTT_QOS_1/*at least once*/, 1/*retain*/ );
    if (!will_rc)
      log.warning( F("failed to set will") );

    const bool keepalive_rc = client->setKeepAliveInterval( MQTT_KEEPALIVE * 1.5 );
    if (!keepalive_rc)
      log.warning( F("failed to set keepalive") );

    for (auto s : subscriptions)
    {
      auto sub = std::make_unique<Adafruit_MQTT_Subscribe>( client, s->sub_topic_buf );
      const bool sub_rc = client->subscribe( sub.get() );
      if (!sub_rc)
        log.warning( F("failed to subscribe to a topic") );
      else
        s->sub = std::move(sub);
    }

    WiFi::register_observer( *this );
  }

  virtual void wifi_down( ) // WifiObserver
  {
    log.info( F("stopping") );
    client->disconnect();

    poll_ticker.detach();
    mqtt_disconnected();
  }

  virtual void wifi_got_ip( ) // WifiObserver
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

  bool publish( const char* topic, const char* payload )
  {
    return client->publish( make_pub_topic(topic), payload );
  }


private:
  WiFiClient my_wifi;
  char client_id_buf[32]; // needs to live as long as the client
  char will_topic_buf[48]; // also needs to live as long as client
  Adafruit_MQTT_Client *client;
  char pub_topic_buf[64];
  int pub_topic_len;
  node::Ticker poll_ticker;
  node::Ticker ping_ticker;

  struct MqttSubscription
  {
    char sub_topic_buf[64];
    std::unique_ptr<Adafruit_MQTT_Subscribe> sub;
  };
  std::vector< std::shared_ptr<MqttSubscription> > subscriptions;

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
  std::vector< std::shared_ptr<MqttHandler> > handlers;

  const char* make_pub_topic( const char *tail )
  {
    char* bp = &pub_topic_buf[pub_topic_len];
    int buf_left = sizeof(pub_topic_buf) - pub_topic_len - 1; // -1 for termination

    my_strncpy( bp, "/",  buf_left ); // separator
    my_strncpy( bp, tail, buf_left );
    *bp = '\0';

    return pub_topic_buf;
  }
  const char* make_will_topic( const char *tail )
  {
    char* bp = &will_topic_buf[0];
    int buf_left = sizeof(will_topic_buf) - 1; // -1 for termination

    my_strncpy( bp, pub_topic_buf, buf_left );
    my_strncpy( bp, "/",  buf_left ); // separator
    my_strncpy( bp, tail, buf_left );
    *bp = '\0';

    return will_topic_buf;
  }

  void mqtt_connected( )
  {
    log.info( F("connected") );

    char buf[32]; // 16 bytes of ipv4 + some
    sprintf( buf, "online %s", my_wifi.localIP().toString().c_str() );
    (void)client->publish( make_pub_topic("connection"), buf, MQTT_QOS_1/*at least once*/, 1/*retain*/ );

    ping_ticker.repeat( MQTT_KEEPALIVE * 1000/*ms*/, [this]() {
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
        poll_ticker.repeat( 100/*ms*/, [this](){ poll( true ); } );
      }
      else
      {
        // the first connection attempt invariably fails as the library
        // starts with a subcription sequence of zero, which is invalid.
        log.info( F("connect failed; will try again") );
        poll_ticker.once( 5000/*ms*/, [this]() { poll( false ); } );
      }
    }
  }

  static Logger log;
};

} // node
