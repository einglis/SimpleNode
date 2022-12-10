
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

// ----------------------------------------------------------------------------

#define NODE_HAS_PIXELS
#define NODE_HAS_NTP
#define NODE_HAS_MQTT
#define NODE_HAS_WEB

// ----------------------------------------------------------------------------

#include "private_config.h" // not in repo...

#define WIFI_SSID PRIVATE_WIFI_SSID
#define WIFI_PASSWD PRIVATE_WIFI_PASSWD
#define WIFI_HOSTNAME "simple_node_host"

#define NTP_HOST "europe.pool.ntp.org"

#define MQTT_HOST PRIVATE_MQTT_HOST
#define MQTT_PORT 1883
#define MQTT_CLIENT "simple_node_client"
#define MQTT_KEEPALIVE  6  // timeout set to 1.5x this value

#define PATTERN_WIFI_DISCONNECTED 0xAAAAAAAA
#define PATTERN_WIFI_CONNECTED    0xF0F0F0F0
#define PATTERN_WIFI_GOT_IP       0xFFFFFFFE

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
    Serial.print( F("Uptime: ") );

    const uint32_t uptime_secs = secs();
    const uint32_t uptime_mins = uptime_secs / 60;
    const uint32_t uptime_hours = uptime_mins / 60;
    const uint32_t uptime_days = uptime_hours / 24;

    #define PLURAL(n) n, (n!=1)?"s":""
    if (uptime_days > 0)
      Serial.printf( "%u day%s, ", PLURAL(uptime_days) ); // can't use F("")
    if (uptime_hours > 0)
      Serial.printf( "%u hour%s", PLURAL(uptime_hours % 24) );
    else if (uptime_mins > 0)
      Serial.printf( "%u minute%s", PLURAL(uptime_mins) );
    else
      Serial.printf( "%u second%s", PLURAL(uptime_secs) );
    #undef PLURAL

    Serial.println( "" );
  }
};

Uptime uptime;

// ----------------------------------------------------------------------------

AsyncWebServer web_server( 80 );
const char* PARAM_MESSAGE = "message";

void web_setup( )
{
  web_server.on("/", HTTP_GET, []( AsyncWebServerRequest* request ) {
    String message;
    message += "Build ";
    message += Version;
    message += "\n";
    message += "Uptime ";
    message += uptime.secs();
    message += " seconds\n";

    request->send(200, "text/plain", message);
  } );

    // Send a GET request to <IP>/get?message=<message>
    web_server.on("/get", HTTP_GET, []( AsyncWebServerRequest* request ) {
        String message;
        if (request->hasParam(PARAM_MESSAGE)) {
            message = request->getParam(PARAM_MESSAGE)->value();
        } else {
            message = "No message sent";
        }
        request->send(200, "text/plain", "Hello, GET: " + message);
    } );

    // Send a POST request to <IP>/post with a form field message set to <message>
    web_server.on("/post", HTTP_POST, []( AsyncWebServerRequest* request ) {
        String message;
        if (request->hasParam(PARAM_MESSAGE, true)) {
            message = request->getParam(PARAM_MESSAGE, true)->value();
        } else {
            message = "No message sent";
        }
        request->send(200, "text/plain", "Hello, POST: " + message);
    } );

  web_server.onNotFound( []( AsyncWebServerRequest* request ) {
    request->send( 404, "text/plain", "Not found" );
  } );
}

void web_loop( )
{
  ;
}

void web_wifi_down( )
{
  Serial.println( F("stopping WebServer") );
  web_server.end();
}

void web_wifi_up( )
{
  Serial.println( F("starting WebServer") );
  web_server.begin();
}
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
      Serial.println( F("MQTT: failed to set will") );

    const bool keepalive_rc = client.setKeepAliveInterval( MQTT_KEEPALIVE * 1.5 );
    if (!keepalive_rc)
      Serial.println( F("MQTT: failed to set keepalive") );


    auto sub = std::make_shared<Adafruit_MQTT_Subscribe>( &client, "node/cmd" );
    sub->setCallback( test_callback );

    const bool sub_rc = client.subscribe( sub.get() );
    if (!sub_rc)
      Serial.println( F("MQTT: failed to subscribe to a topic") );
    else
      subs.push_back( sub );

    WifiObservers::add( this );
  }

  virtual void wifi_down( ) // WifiObserver
  {
    Serial.println( F("MQTT: stopping") );
    client.disconnect();

    poll_ticker.detach();
    mqtt_disconnected();
  }

  virtual void wifi_up( ) // WifiObserver
  {
    Serial.println( F("MQTT: starting") );
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
    Serial.print("MQTT: message... ");
    Serial.println(data);
  }

  void mqtt_connected( )
  {
    Serial.println( F("MQTT: connected") );
    ping_ticker.attach_scheduled( MQTT_KEEPALIVE, [this]() {
      Serial.println( F("MQTT: ping") );
      client.ping(); // can block for up to 500 ms.
    } );
  }
  void mqtt_disconnected( )
  {
    Serial.println( F("MQTT: disconnected") );
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

      Serial.println( F("MQTT: trying to connect") );
      if (client.connect() == 0) /* 0 == connected*/
      {
        mqtt_connected();
        poll_ticker.attach_ms_scheduled( 100, [this](){ poll( true ); } );
      }
      else
      {
        // the first connection attempt invariably fails as the library
        // starts with a subcription sequence of zero, which is invalid.
        Serial.println( F("MQTT: connect failed; will try again") );
        poll_ticker.once_scheduled( 5, [this](){ poll( false ); } );
      }
    }
  }
};

Mqtt mqtt;

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
      Serial.print( F("NTP time: ") );
      Serial.println( client.getFormattedTime() );
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
    Serial.println( F("NTP refresh") );

    int wait;
    if (client.update())
    {
      wait = 10 * 60; // ten minutes after success
      phase = 0; // restart at 1 second on next fail
    }
    else
    {
      wait = 1 << phase; // 1, 2, 4,... seconds
      phase = min( ++phase, 5 ); // 1 << 5 == 32 seconds
    }

    refresh_ticker.once_scheduled( wait, [this, phase](){ refresh( phase ); } );
  }
};

Ntp ntp;

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
    Serial.print( F("Wifi MAC: ") );
    Serial.println(WiFi.macAddress().c_str());

    WiFi.hostname( WIFI_HOSTNAME );
    WiFi.persistent(false); // don't stash config in Flash

    handlers.push_back( WiFi.onStationModeDisconnected( [this](auto e){ wifi_disconnected(e); } ) );
    handlers.push_back( WiFi.onStationModeConnected( [this](auto e){ wifi_connected(e); } ) );
    handlers.push_back( WiFi.onStationModeGotIP( [this](auto e){ wifi_got_ip(e); } ) );

    Serial.println( F("Looking for WiFi...") );
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
    Serial.println( F("WiFi disconnected") );
    is_connected = false;

    schedule_function( []() {
  #ifdef NODE_HAS_WEB
      web_wifi_down();
  #endif

      WifiObservers::wifi_down();
    } );
  }

  void wifi_connected( const WiFiEventStationModeConnected & )
  {
    patterns.set( PATTERN_WIFI_CONNECTED );
    Serial.println( F("WiFi connected") );
    is_connected = true;
  }

  void wifi_got_ip( const WiFiEventStationModeGotIP &e )
  {
    patterns.set( PATTERN_WIFI_GOT_IP );
    Serial.print( F("WiFi got IP: ") );
    Serial.println( e.ip );

    schedule_function( []() {
  #ifdef NODE_HAS_WEB
      web_wifi_up();
  #endif

      WifiObservers::wifi_up();
    } );
  }
};

NodeWiFi wifi;

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_PIXELS

#define PIXELS_PIN  13 // 13: dev, 14: island
#define NUMPIXELS  251 // deliberately stressful

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

void setup( )
{
  Serial.begin(57600);
  Serial.println("");
  Serial.println("");
  Serial.println( Version );
  Serial.println( ESP.getResetReason() );

  uptime.setup();
  patterns.setup();
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
  web_setup();
#endif
}

void loop( )
{
  static long last = millis();
  static int loops = 0;

  loops++;
  long now = millis();
  if (now - last > 1000)
  {
    Serial.print( F("Loop rate is about ") );
    Serial.print( loops * 1000 / (now-last) );
    Serial.println( F(" Hz") );
    loops = 0;
    last = now;
  }

  Loopies::exec();

#ifdef NODE_HAS_WEB
  web_loop();
#endif
}
