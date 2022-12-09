
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

struct SetupAndLoop
{
  virtual void setup() { };
  virtual void loop() { };
  virtual ~SetupAndLoop() = 0;
};

SetupAndLoop::~SetupAndLoop( ) { }
  // pure virtual destructor implementation


class SetupAndLoopManager // don't like this naming
{
public:
  static void add( SetupAndLoop& sal )
  {
    list.push_back( &sal );
    sal.setup();
  }
  static void exec( )
  {
    for ( auto sal : list )
      sal->loop();
  }
private:
  static std::vector< SetupAndLoop* > list; // declaration...
};

std::vector< SetupAndLoop* > SetupAndLoopManager::list; // ...definition

// ----------------------------------------------------------------------------

class Patterns
  : public SetupAndLoop
  //, public Singleton
{
public:
  Patterns( int pin = LED_BUILTIN )
    : pin{ pin }
    , pattern{ 0 }
  { }

  virtual void setup()
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
  : public SetupAndLoop
  //, public Singleton
{
public:
  Uptime( )
    : counter{ 0 }
  { }

  virtual void setup( )
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

WiFiClient mqtt_wifi;
Adafruit_MQTT_Client* mqtt_client = nullptr;
  // would be pretty benign to have this statically initialised; nothing major happens
Ticker mqtt_poll_ticker;
Ticker mqtt_ping_ticker;

void mqtt_connected( )
{
  Serial.println( F("MQTT connected") );
  mqtt_ping_ticker.attach_scheduled( MQTT_KEEPALIVE, []() {
    Serial.println( F("MQTT ping") );
    mqtt_client->ping(); // can block for up to 500 ms.
  } );
}
void mqtt_disconnected( )
{
  Serial.println( F("MQTT disconnected") );
  mqtt_ping_ticker.detach();
}


void mqtt_test_callback(char *data, uint16_t len) {
  Serial.print("Hey we're in an mqtt callback: ");
  Serial.println(data);
}

void mqtt_setup( )
{
  mqtt_client = new Adafruit_MQTT_Client( &mqtt_wifi, MQTT_HOST, MQTT_PORT, MQTT_CLIENT, ""/*key*/ );

  static Adafruit_MQTT_Subscribe feed = Adafruit_MQTT_Subscribe( mqtt_client, "node/cmd" );

  feed.setCallback( mqtt_test_callback );
  bool sub_rc = mqtt_client->subscribe( &feed );
  bool will_rc = mqtt_client->will( "will_topic", "will_payload" );

  mqtt_client->setKeepAliveInterval( MQTT_KEEPALIVE * 1.5 );
}

void mqtt_loop( )
{
  Adafruit_MQTT_Subscribe *sub = mqtt_client->readSubscription( 0/*timeout*/ );
  if (sub)
    mqtt_client->processSubscriptionPacket(sub);
}

void mqtt_poll( bool was_connected = false )
{
  const bool is_connected = mqtt_client->connected();
  if (is_connected ^ was_connected)
  {
    if (is_connected)
      mqtt_connected();
    else
      mqtt_disconnected();
  }

  int wait = 1; // default poll interval

  if (!is_connected)
  {
    Serial.println( F("trying to connect to MQTT") );
    if (mqtt_client->connect() != 0) /* 0 == connected*/
    {
      // first connection attempt inevitably fails as the library
      // starts with a subcription sequence of zero, which is invalid.
      Serial.println( F("failed; try again in five") );
      wait = 5; // rate-limit re-connection
    }
  }

  mqtt_poll_ticker.once_scheduled( wait, [is_connected](){ mqtt_poll( is_connected ); } );
    // is_connected might still be false, even if we've just re-connected, but this
    // will allow us to notice the change next time around.  If we disconnect again
    // in the meantime, we _won't_ notice the change, but that's logically fine.
}

void mqtt_wifi_down( )
{
  Serial.println( F("stopping MQTT") );
  mqtt_client->disconnect();

  mqtt_poll_ticker.detach();
}

void mqtt_wifi_up( )
{
  Serial.println( F("starting MQTT") );
  mqtt_poll();
}

#endif

// ----------------------------------------------------------------------------

struct WifiObserver
{
  virtual void wifi_up() { };
  virtual void wifi_down() { };
  virtual ~WifiObserver() = 0;
};

WifiObserver::~WifiObserver( ) { }
  // pure virtual destructor implementation

class WifiObservers
{
public:
  static void add( WifiObserver& wo )
  {
    list.push_back( &wo );
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



#ifdef NODE_HAS_NTP

class Ntp
  : public SetupAndLoop
  , public WifiObserver
  //, public Singleton
{
public:
  Ntp()
    : client( my_wifi, NTP_HOST/*lazy*/ )
  { }

  virtual void setup()
  {
    WifiObservers::add( *this );

    report_ticker.attach_scheduled( 11, [this](){
      Serial.print( F("NTP time: ") );
      Serial.println( client.getFormattedTime() );
    } );
  }

  virtual void wifi_up()
  {
    client.begin();
    refresh();
  }

  virtual void wifi_down()
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

void wifi_disconnected( const WiFiEventStationModeDisconnected & )
{
  patterns.set( PATTERN_WIFI_DISCONNECTED );
  Serial.println( F("WiFi disconnected") );

  schedule_function( []() {
#ifdef NODE_HAS_MQTT
    mqtt_wifi_down();
#endif
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
}

void wifi_got_ip( const WiFiEventStationModeGotIP &e )
{
  patterns.set( PATTERN_WIFI_GOT_IP );
  Serial.print( F("WiFi got IP: ") );
  Serial.println( e.ip );

  schedule_function( []() {
#ifdef NODE_HAS_MQTT
    mqtt_wifi_up();
#endif
#ifdef NODE_HAS_WEB
    web_wifi_up();
#endif

    WifiObservers::wifi_up();
  } );
}

void wifi_setup( )
{
  Serial.print( F("Wifi MAC: ") );
  Serial.println(WiFi.macAddress().c_str());

  WiFi.hostname( WIFI_HOSTNAME );
  WiFi.persistent(false); // don't stash config in Flash

  static WiFiEventHandler d = WiFi.onStationModeDisconnected( wifi_disconnected );
  static WiFiEventHandler c = WiFi.onStationModeConnected( wifi_connected );
  static WiFiEventHandler g = WiFi.onStationModeGotIP( wifi_got_ip );

  Serial.println( F("Looking for WiFi...") );

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    // it transpires that this will keep looking forever,
    // _and_ try to reconnect if the connection is lost.
}

void wifi_loop( )
{
  ;
}

// ----------------------------------------------------------------------------

#ifdef NODE_HAS_PIXELS

#define PIXELS_PIN  13 // 13: dev, 14: island
#define NUMPIXELS  251 // deliberately stressful

static uint32_t Wheel(byte WheelPos) {
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

Adafruit_NeoPixel* pixels = nullptr;
Ticker pixel_ticker;

void pixel_update( )
{
  static bool ping_pong = true;
    // split the work in two two chunks to have some 
    // tiny smoothing effect on the system load.

  if (ping_pong)
  {
    static uint32_t j = 0;
    j++;

    const uint32_t colour = Wheel( j & 0xff );
    for (int i = 0; i < pixels->numPixels(); i++)
        pixels->setPixelColor( i, colour );
  }
  else
  {
    pixels->show();
  }

  ping_pong = !ping_pong;
}

void pixels_setup( )
{
  pinMode( PIXELS_PIN, OUTPUT );

  pixels = new Adafruit_NeoPixel( NUMPIXELS,PIXELS_PIN, NEO_GRB + NEO_KHZ800 );

  pixels->begin(); // This initializes the NeoPixel library.
  pixels->setBrightness( 4 );

  pixel_ticker.attach( 0.1, [](){ pixel_update(); } );
}

void pixels_loop( )
{
  ;
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

  SetupAndLoopManager::add( uptime );
  SetupAndLoopManager::add( patterns );
  patterns.set( PATTERN_WIFI_DISCONNECTED );

  wifi_setup();
#ifdef NODE_HAS_NTP
  SetupAndLoopManager::add( ntp );
#endif
#ifdef NODE_HAS_MQTT
  mqtt_setup();
#endif
#ifdef NODE_HAS_PIXELS
  pixels_setup();
#endif
#ifdef NODE_HAS_WEB
  web_setup();
#endif
}

void loop( )
{
  SetupAndLoopManager::exec();

  //wifi_loop();
#ifdef NODE_HAS_MQTT
  mqtt_loop();
#endif
#ifdef NODE_HAS_PIXELS
  //pixels_loop();
#endif
#ifdef NODE_HAS_WEB
  web_loop();
#endif
}
