
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

struct WifiObserver
{
  virtual void wifi_up() { };
  virtual void wifi_down() { };
  virtual ~WifiObserver() = 0;
};

WifiObserver::~WifiObserver( ) { }
  // pure virtual destructor implementation

// ------------------------------------

class WifiObservers
{
public:
  static void add( WifiObserver* wo )
  {
    list.push_back( wo );
  }
  static void wifi_up( )
  {
    for ( auto wo : list )
      wo->wifi_up();
  }
  static void wifi_down( )
  {
    for ( auto wo : list )
      wo->wifi_down();
  }
private:
  static std::vector< WifiObserver* > list; // declaration...
};

std::vector< WifiObserver* > WifiObservers::list; // ...definit

// ----------------------------------------------------------------------------

class Patterns
{
public:
  Patterns( int pin = LED_BUILTIN )
    : pin{ pin }
    , pattern{ 0 }
  { }

  void setup()
  {
    pinMode( pin, OUTPUT );

    ticker.attach_ms( 100, [this]() {
      pattern = (pattern >> 1) | (pattern << 31); // roll right 1
      digitalWrite( pin, ~(pattern) & 1 );
    } );
  }

  void set( uint32_t p )
  {
    pattern = p;
  }

private:
  const int pin;
  uint32_t pattern;
  Ticker ticker;
};

Patterns patterns;

// ----------------------------------------------------------------------------

class Uptime
{
public:
  Uptime( )
    : counter{ 0 }
  { }

  void setup( )
  {
    update_ticker.attach( 1, [this](){ counter++; } );
    report_ticker.attach( 6, [this](){ report(); } );
  }

  uint32_t secs( ) { return counter; }

private:
  uint32_t counter; // 32 bits is enough for 136 years of seconds
  Ticker update_ticker;
  Ticker report_ticker;

  void report( )
  {
    const uint32_t uptime_secs = secs();
    const uint32_t uptime_mins = uptime_secs / 60;
    const uint32_t uptime_hours = uptime_mins / 60;
    const uint32_t uptime_days = uptime_hours / 24;

    #define PLURAL(n) n, (n!=1)?"s":""
    if (uptime_days > 0)
      log.debugf( "%u day%s, ", PLURAL(uptime_days) );
    if (uptime_hours > 0)
      log.debugf( "%u hour%s", PLURAL(uptime_hours % 24) );
    else if (uptime_mins > 0)
      log.debugf( "%u minute%s", PLURAL(uptime_mins) );
    else
      log.debugf( "%u second%s", PLURAL(uptime_secs) );
    #undef PLURAL
  }

  static Logger log;
};

Uptime uptime;
Logger Uptime::log( "UPTIME" );

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_CONFIG

#define CONFIG_MAGIC 0x20f4da53

union ConfigHeader
{
  struct {
    uint32_t magic;
    size_t length;
  };
  uint8_t padding[32]; // gentle future proof
};

class Configuration
{
public:
  Configuration( const char* filename = CONFIG_FILENAME )
    : filename{ filename }
    , valid{ false }
  { }

  void setup()
  {
    if (!LittleFS.begin())
      log.error( "failed to mount file system" );

    (void)load( );
  }

  bool is_valid() const { return valid; }

  AppConfig& rw() { return app_config; }
  const AppConfig& ro() { return app_config; }
  const AppConfig& operator()() { return ro(); }

  bool save( )
  {
    File cf = LittleFS.open( filename, "w" ); // "wb" not allowed, apparently
    if (!cf)
    {
      log.error( "cannot open config for writing" );
      // does not necessarily invalidate the config
      return false;
    }

    bool save_success = false;

    ConfigHeader hdr;
    memset( &hdr, 0, sizeof(hdr) );
    hdr.magic = CONFIG_MAGIC;
    hdr.length = sizeof(app_config);

    const size_t write_rc = cf.write( (const uint8_t*)&hdr, sizeof(hdr) );
    if (write_rc == sizeof(hdr))
    {
      const size_t write_rc = cf.write( (const uint8_t*)&app_config, hdr.length );
      save_success = (write_rc == hdr.length);
      if (save_success)
        log.debugf( "successfully wrote %u bytes of config", write_rc );
      else
        log.errorf( "wrote %u bytes, not %u, of config", write_rc, hdr.length );
    }
    else
    {
      log.errorf( "wrote %u bytes, not %u, of config", write_rc, sizeof(hdr) );
    }

    cf.close();

    valid = save_success; // implicitly valid once saved
    return save_success;
  }

  bool load( )
  {
    File cf = LittleFS.open( filename, "r" );
    if (!cf)
    {
      log.error( "cannot open config for reading" );
      // does not necessarily invalidate the config
      return false;
    }

    bool load_success = false;

    ConfigHeader hdr;

    const size_t read_rc = cf.read( (uint8_t*)&hdr, sizeof(hdr) );
    if (read_rc == sizeof(hdr))
    {
      if (hdr.magic == CONFIG_MAGIC)
      {
        if (hdr.length != sizeof(app_config))
          log.warningf( "length %u does not match config size %u", hdr.length, sizeof(app_config) );
            // not necessarily terminal; eg if config has changed length between versions; caveat emptor.

        hdr.length = min( hdr.length, sizeof(app_config) );
          // just in case the config has got shorter, don't want to overflow the structure

        const size_t read_rc = cf.read( (uint8_t*)&app_config, hdr.length );
        load_success = (read_rc == hdr.length);
        if (load_success)
          log.debugf( "successfully read %u bytes of config", read_rc );
        else
          log.errorf( "read %u bytes, not %u, of config", read_rc, hdr.length );
      }
      else
      {
        log.warningf( "header magic %08x not expected %08x", hdr.magic, CONFIG_MAGIC );
      }
    }
    else
    {
      log.errorf( "read %u bytes, not %u, of hdr", read_rc, sizeof(hdr) );
    }

    cf.close();

    valid = load_success;
    return load_success;
  }

private:
  const char *filename;
  AppConfig app_config;
  bool valid;

  static Logger log;
};

Configuration configuration;
Logger Configuration::log( "CONFIG" );

#endif

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

    schedule_function( []() { WifiObservers::wifi_down(); } );
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

    schedule_function( []() { WifiObservers::wifi_up(); } );
  }

  static Logger log;
};

NodeWiFi wifi;
Logger NodeWiFi::log( "WIFI" );

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_WEB

class Webserver
  : public WifiObserver
{
public:
  Webserver()
    : server( WEBSERVER_PORT/*lazy*/ )
    { }

  void setup( )
  {
    server.onNotFound( [](auto request) { request->send( 404, "text/plain", "Not found" ); } );
    WifiObservers::add( this );
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
  : public WifiObserver
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

    WifiObservers::add( this );
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
  : public WifiObserver
{
public:
  Ntp()
    : client( my_wifi, NTP_HOST/*lazy*/ )
  { }

  void setup()
  {
    report_ticker.attach_scheduled( 11, [this](){
      log.infof( "time: %s", client.getFormattedTime() );
    } );

    WifiObservers::add( this );
  }

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
  ntp.setup();
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
