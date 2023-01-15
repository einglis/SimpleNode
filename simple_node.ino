
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <Ticker.h>
#include <WiFiUdp.h>

#include "app_config.h"
#include "build.gen.h"
const char *Version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";
#include "logging.h"

// ----------------------------------------------------------------------------

#include "node/wifi_observer.h"

#include "node/patterns.h"
node::Patterns patterns;

#include "node/uptime.h"
node::Uptime uptime;

#include "node/configuration.h"
node::Configuration configuration;


// ----------------------------------------------------------------------------

class NodeWiFi
{
public:
  NodeWiFi( )
    : is_connected{ false }
    { }

  void setup()
  {
    log.infof( "MAC: %s", WiFi.macAddress().c_str() );

    WiFi.hostname( WIFI_HOSTNAME );
    WiFi.persistent(false); // don't stash config in Flash

    handlers.push_back( WiFi.onStationModeDisconnected( [this](auto e){ wifi_disconnected(e); } ) );
    handlers.push_back( WiFi.onStationModeConnected( [this](auto e){ wifi_connected(e); } ) );
    handlers.push_back( WiFi.onStationModeGotIP( [this](auto e){ wifi_got_ip(e); } ) );

    log.info( F("inital connection...") );
    patterns.set( PATTERN_WIFI_DISCONNECTED );

    WiFi.mode( WIFI_STA );
    WiFi.begin( WIFI_SSID, WIFI_PASSWD );
      // it transpires that this will keep looking forever,
      // _and_ try to reconnect if the connection is lost.
  }

  IPAddress ip() { return my_ip; }

private:
  std::vector< WiFiEventHandler > handlers;
  bool is_connected;
  IPAddress my_ip;

  void wifi_disconnected( const WiFiEventStationModeDisconnected& )
  {
    if (!is_connected) // this function tends to get called over-and-over
      return

    patterns.set( PATTERN_WIFI_DISCONNECTED );
    log.info( F("disconnected") );
    is_connected = false;
    my_ip.clear();

    node::WifiObservers::wifi_down();
  }

  void wifi_connected( const WiFiEventStationModeConnected & )
  {
    patterns.set( PATTERN_WIFI_CONNECTED );
    log.info( F("connected") );
    is_connected = true;
  }

  void wifi_got_ip( const WiFiEventStationModeGotIP &e )
  {
    patterns.set( PATTERN_WIFI_GOT_IP );
    log.infof( "got IP: %s", e.ip.toString().c_str() );
    my_ip = e.ip;

    node::WifiObservers::wifi_up();
  }

  static Logger log;
};

NodeWiFi wifi;
Logger NodeWiFi::log( "WIFI" );

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_WEB

class Webserver
  : public node::WifiObserver
{
public:
  Webserver()
    : server( WEBSERVER_PORT/*lazy*/ )
    { }

  void setup( )
  {
    server.onNotFound( [](auto request) { request->send( 404, "text/plain", "Not found" ); } );
    node::WifiObservers::add( this );
  }

  void add_handler( const char* uri, WebRequestMethod method, ArRequestHandlerFunction fn, ArUploadHandlerFunction upload = nullptr )
  {
    handlers.push_back( server.on( uri, method, fn, upload ) );
  }

  virtual void wifi_down() // WifiObserver
  {
    log.info( F("stopping") );
    server.end();
  }

  virtual void wifi_up() // WifiObserver
  {
    log.info( F("starting") );
    server.begin();
  }

private:
  AsyncWebServer server;
  std::vector< AsyncCallbackWebHandler > handlers;
    // raw ptrs rather than shared mostly to cut down on syntactic noise
    // but it's irrelevant anyway, since we never expect these to be deleted.

  static Logger log;
};

Webserver web;
Logger Webserver::log( "WEB" );

#endif

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_MQTT

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

Mqtt mqtt;
Logger Mqtt::log( "MQTT" );

#endif

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_NTP

class Ntp
  : public node::WifiObserver
{
public:
  Ntp()
    : client( my_wifi, NTP_HOST/*lazy*/ )
  { }

  void setup( int report_interval = 0 )
  {
    if (report_interval)
      report_ticker.attach_scheduled( report_interval, [this](){
        log.infof( "time: %s", client.getFormattedTime() );
      } );

    node::WifiObservers::add( this );
  }

  long int epoch_time( ) { return client.getEpochTime(); }

  static int epoch_day( long int e )  { return (e / 86400L + 4) % 7; } // 0 == sunday
  static int epoch_hrs( long int e )  { return (e / 3600) % 24; }
  static int epoch_mins( long int e ) { return (e / 60) % 60; }
  static int epoch_secs( long int e ) { return  e % 60; }

  virtual void wifi_up() // WifiObserver
  {
    client.begin();
    refresh();
  }

  virtual void wifi_down() // WifiObserver
  {
    refresh_ticker.detach();
    client.end();
  }

private:
  WiFiUDP my_wifi;
  NTPClient client;
  Ticker report_ticker;
  Ticker refresh_ticker;

  void refresh( int phase = 0 )
  {
    log.debug( F("refresh") );

    int wait;
    if (client.update())
    {
      wait = 10 * 60; // ten minutes after success
      phase = 0; // restart at 1 second on next fail
    }
    else
    {
      wait = 1 << phase; // 1, 2, 4,... seconds
      phase = min( phase + 1, 5 ); // 1 << 5 == 32 seconds
    }

    refresh_ticker.once_scheduled( wait, [this, phase](){ refresh( phase ); } );
  }

  static Logger log;
};

Ntp ntp;
Logger Ntp::log( "NTP" );

#endif

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_PIXELS

class Pixels
{
public:
  Pixels( int num_pixels = NUMPIXELS, int pin = PIXELS_PIN ) // lazy
    : pixels( num_pixels, pin, NEO_GRB + NEO_KHZ800 )
    , work_phase{ 0 }
    , need_update{ false }
    { }

  void setup()
  {
    //pinMode( pin, OUTPUT );
      // the Adafruit_NeoPixel constructor will have done this for us

    pixels.begin(); // This initializes the NeoPixel library.

    ticker.attach_ms( 50, [this](){ update(); } );
  }

  void brightness( uint8_t b )
  {
    pixels.setBrightness( b );
  }

private:
  Adafruit_NeoPixel pixels;
  Ticker ticker;
  int work_phase;
  bool need_update;

  void update( )
  {
    // called in SYS context, so be quick

    if (work_phase == 0)
    {
      need_update = app_pixels_update( pixels.numPixels(),
        [this](uint16_t n, uint32_t c){ pixels.setPixelColor( n, c ); } );
    }
    else
    {
      if (need_update)
        pixels.show();
    }

    work_phase = !work_phase;
  }

};

Pixels pixels;

#endif

// ----------------------------------------------------------------------------

void setup( )
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println( Version );
  Serial.println( ESP.getResetReason() );

  uptime.setup();
  patterns.setup();

#ifdef NODE_HAS_CONFIG
  configuration.setup();
#endif

  wifi.setup();

#ifdef NODE_HAS_NTP
  ntp.setup( 11 /*report interval in seconds*/ );
#endif
#ifdef NODE_HAS_MQTT
  mqtt.setup( );
#endif
#ifdef NODE_HAS_PIXELS
  pixels.setup();
#endif
#ifdef NODE_HAS_WEB
  web.setup();
  register_web_pages( web );
#endif

  app_setup();
}

void loop( )
{
  static Logger log( "LOOP" );
  static long last = millis();
  static int loops = 0;

  loops++;
  long now = millis();
  if (now - last > 7000) // magic, but arbitrary number
  {
    log.debugf( "loop rate is about %d Hz", loops * 1000 / (now-last) );
    loops = 0;
    last = now;
  }
}
