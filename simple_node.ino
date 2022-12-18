
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <Ticker.h>
#include <WiFiUdp.h>

#include "build.gen.h"
const char *Version = XXX_BUILD_REPO_VERSION " (" XXX_BUILD_DATE ")";

#include "logging.h"

// ----------------------------------------------------------------------------

#define NODE_HAS_PIXELS  // ~3 kHz cost
#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_INPUTS

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_INPUTS
#include "inputs.h"
#endif

// ----------------------------------------------------------------------------

#include "private_config.h" // not in repo...

#define WIFI_SSID PRIVATE_WIFI_SSID
#define WIFI_PASSWD PRIVATE_WIFI_PASSWD
#define WIFI_HOSTNAME "simple_node_host"

#define PATTERN_WIFI_DISCONNECTED 0xAAAAAAAA
#define PATTERN_WIFI_CONNECTED    0xF0F0F0F0
#define PATTERN_WIFI_GOT_IP       0xFFFFFFFE

#define PIXELS_PIN 13 // 13: dev, 14: island
#define NUMPIXELS 251 // deliberately stressful

#define NTP_HOST "europe.pool.ntp.org"

#define MQTT_HOST PRIVATE_MQTT_HOST
#define MQTT_PORT 1883
#define MQTT_CLIENT "simple_node_client"
#define MQTT_KEEPALIVE 60  // timeout set to 1.5x this value

#define WEBSERVER_PORT 80

#define LOOP_RATE_CHECK_INTERVAL_MS 7000

// ----------------------------------------------------------------------------

struct Loopy
{
  virtual void loop() = 0;
  virtual ~Loopy() = 0;
};

Loopy::~Loopy( ) { }
  // pure virtual destructor implementation

// ------------------------------------

class Loopies
{
public:
  static void add( Loopy* l )
  {
    list.push_back( l );
  }
  static void exec( )
  {
    for ( auto l : list )
      l->loop();
  }
private:
  static std::vector< Loopy* > list; // declaration...
};

std::vector< Loopy* > Loopies::list; // ...definition

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
    handlers.push_back( server.on("/", HTTP_GET, [this](auto r){ handle_default(r); } ) );
    handlers.push_back( server.on("/get", HTTP_GET, [this](auto r){ handle_get_demo(r); } ) );
    handlers.push_back( server.on("/post", HTTP_POST, [this](auto r){ handle_post_demo(r); } ) );

    server.onNotFound( [this](auto r){ handle_not_found(r); } );

    WifiObservers::add( this );
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

  // ----------------------------------

  void handle_default( AsyncWebServerRequest* request )
  {
    String message;
    message += "Build ";
    message += Version;
    message += "\n";
    message += "Uptime ";
    message += uptime.secs();
    message += " seconds\n";

    request->send(200, "text/plain", message);
  }

  #define PARAM_MESSAGE "message"

  void handle_get_demo( AsyncWebServerRequest* request )
  {
    // Send a GET request to <IP>/get?message=<message>
    String message;
    if (request->hasParam(PARAM_MESSAGE))
      message = request->getParam(PARAM_MESSAGE)->value();
    else
      message = "No message sent";

    request->send(200, "text/plain", "Hello, GET: " + message);
  }

  void handle_post_demo( AsyncWebServerRequest* request )
  {
    // Send a POST request to <IP>/post with a form field message set to <message>
    String message;
    if (request->hasParam(PARAM_MESSAGE, true/*is_post*/))
        message = request->getParam(PARAM_MESSAGE, true)->value();
    else
        message = "No message sent";

    request->send(200, "text/plain", "Hello, POST: " + message);
  }

  #undef PARAM_MESSAGE

  void handle_not_found( AsyncWebServerRequest* request )
  {
    request->send( 404, "text/plain", "Not found" );
  }

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
    const bool will_rc = client.will( "will_topic", "will_payload" );
    if (!will_rc)
      log.warning( F("failed to set will") );

    const bool keepalive_rc = client.setKeepAliveInterval( MQTT_KEEPALIVE * 1.5 );
    if (!keepalive_rc)
      log.warning( F("failed to set keepalive") );


    auto sub = std::make_shared<Adafruit_MQTT_Subscribe>( &client, "node/cmd" );
    sub->setCallback( test_callback );

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

private:
  WiFiClient my_wifi;
  Adafruit_MQTT_Client client;
  Ticker poll_ticker;
  Ticker ping_ticker;
  std::vector< std::shared_ptr<Adafruit_MQTT_Subscribe> > subs;

  static void test_callback( char *data, uint16_t len )
  {
    log.infof( "message of len %u: %s", len, data );
  }

  void mqtt_connected( )
  {
    log.info( F("connected") );
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

  void poll( bool was_connected = false )
  {
    if (client.connected())
    {
      Adafruit_MQTT_Subscribe *sub = client.readSubscription( 0/*timeout*/ );
      for (int i = 0; sub && i < 10; i++) // i count is to limit potential batch size
      {
        client.processSubscriptionPacket( sub );
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

private:
  std::vector< WiFiEventHandler > handlers;
  bool is_connected;

  void wifi_disconnected( const WiFiEventStationModeDisconnected& )
  {
    if (!is_connected) // this function tends to get called over-and-over
      return

    patterns.set( PATTERN_WIFI_DISCONNECTED );
    log.info( F("disconnected") );
    is_connected = false;

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

    schedule_function( []() { WifiObservers::wifi_up(); } );
  }

  static Logger log;
};

NodeWiFi wifi;
Logger NodeWiFi::log( "WIFI" );

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_PIXELS

class Pixels
{
public:
  Pixels( int num_pixels = NUMPIXELS, int pin = PIXELS_PIN ) // lazy
    : pixels( num_pixels, pin, NEO_GRB + NEO_KHZ800 )
    , work_phase{ 0 }
    , pattern_phase{ 0 }
    { }

  void setup()
  {
    //pinMode( pin, OUTPUT );
      // the Adafruit_NeoPixel constructor will have done this for us

    pixels.setBrightness( 4 );
    pixels.begin(); // This initializes the NeoPixel library.

    ticker.attach( 0.1, [this](){ update(); } );
  }

private:
  Adafruit_NeoPixel pixels;
  Ticker ticker;
  int work_phase;
  uint8_t pattern_phase;

  void update( )
  {
    // called in SYS context, so be quick

    if (work_phase == 0)
    {
      pattern_phase++; // happy to overflow and loop

      for (int i = 0; i < pixels.numPixels(); i++)
          pixels.setPixelColor( i, Wheel( pattern_phase ) );
    }
    else
    {
      pixels.show();
    }

    work_phase = !work_phase;
  }

  uint32_t Wheel(byte WheelPos)
  {
    if(WheelPos < 85) {
      return Adafruit_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if(WheelPos < 170) {
      WheelPos -= 85;
      return Adafruit_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
      WheelPos -= 170;
      return Adafruit_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
  }
};

Pixels pixels;

#endif

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_INPUTS

ButtonInput db( [](){ return !digitalRead(0); /*active low*/ } );
SwitchInput ds( [](){ return !digitalRead(12); /*active low*/ } );

Logger input_log( "INPUTS" );

void button_event( ButtonInput::Event f, int count ) // called in SYS context
{
  schedule_function( [=]() {
    switch (f)
    {
      case ButtonInput::Press:
          input_log.infof( "Button press %d", count );
          break;
      case ButtonInput::HoldShort:
          input_log.infof( "Button hold short (%d)", count );
          break;
      case ButtonInput::HoldLong:
          input_log.infof( "Button hold long (%d)", count );
          break;
      case ButtonInput::Final:
          input_log.infof( "Button final (%d)", count );
          break;
      default:
          break; // most unexpected.
    }
  } );
}
void switch_event( SwitchInput::Event f, int count ) // called in SYS context
{
  schedule_function( [=]() {
    switch (f)
    {
      case SwitchInput::FlipOpen:
          input_log.infof( "Switch flip open (%d)", count );
          break;
      case SwitchInput::FlipClose:
          input_log.infof( "Switch flip close (%d)", count );
          break;
      case SwitchInput::Final:
          input_log.infof( "Switch final (%d)", count );
          break;
      default:
          break; // most unexpected.
    }
  } );
}

#endif

// ----------------------------------------------------------------------------

void setup( )
{
  Serial.begin(57600);
  Serial.println("");
  Serial.println("");
  Serial.println( Version );
  Serial.println( ESP.getResetReason() );

  uptime.setup();
  patterns.setup();

#ifdef NODE_HAS_INPUTS
  ds.setup( switch_event );
  db.setup( button_event );
#endif

  wifi.setup();

#ifdef NODE_HAS_NTP
  ntp.setup();
#endif
#ifdef NODE_HAS_MQTT
  mqtt.setup();
#endif
#ifdef NODE_HAS_PIXELS
  pixels.setup();
#endif
#ifdef NODE_HAS_WEB
  web.setup();
#endif
}

void loop( )
{
  static Logger log( "LOOP" );
  static long last = millis();
  static int loops = 0;

  loops++;
  long now = millis();
  if (now - last > LOOP_RATE_CHECK_INTERVAL_MS)
  {
    log.debugf( "loop rate is about %d Hz", loops * 1000 / (now-last) );
    loops = 0;
    last = now;
  }

  Loopies::exec();
}
