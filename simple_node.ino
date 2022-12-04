
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

std::vector< std::function<void(void)> > ms_callbacks;

void IRAM_ATTR one_ms_callback( )
{
  std::for_each( ms_callbacks.begin(), ms_callbacks.end(), [](auto f){ f(); } );
}

void ms_callback_setup( )
{
  timer1_isr_init();
  timer1_write(313); // 1ms interrupt
  timer1_attachInterrupt(one_ms_callback);
  timer1_enable(TIM_DIV256, TIM_EDGE, TIM_LOOP);
}

void ms_callback_loop( )
{
  ;
}

void ms_callback_add( std::function< void(void) > f )
{
  // _probably_ don't need to protect this from concurrent access...
  ms_callbacks.push_back( f );
}

// ----------------------------------------------------------------------------

static volatile uint32_t pattern = PATTERN_WIFI_DISCONNECTED;
static volatile int pattern_count = 0;

void IRAM_ATTR patterns_ms_callback( void )
{
  if (pattern_count++ == 100)
  {
    pattern = (pattern >> 1) | (pattern << 31); // roll right 1
    digitalWrite(LED_BUILTIN, ~pattern & 1);
    pattern_count = 0;
  }
}

void patterns_setup( )
{
  pinMode( LED_BUILTIN, OUTPUT );
  ms_callback_add( patterns_ms_callback );
    // I could have done this as a lambda, but felt like using
    // the IRAM_ATTR (because it seemed like a good idea) which
    // meant I really needed a separate function.
}

void patterns_loop( )
{
  ;
}

// ----------------------------------------------------------------------------

static uint32_t uptime_secs = 0; // 136 years
Ticker uptime_ticker;

void uptime_setup( )
{
  uptime_ticker.attach( 1, [](){ uptime_secs++; } );
}

void uptime_loop( )
{
  static uint32_t last_uptime = 0;
  if (uptime_secs - last_uptime > 5)
  {
    Serial.println(uptime_secs);
    last_uptime = uptime_secs;
  }
}

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
    message += uptime_secs;
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

#ifdef NODE_HAS_NTP

WiFiUDP ntpUDP;
NTPClient ntp_client( ntpUDP, NTP_HOST );

Ticker ntp_ticker;

void ntp_setup( )
{
  ;
}

void ntp_loop( )
{
  static long last = millis();
  if (millis() - last > 10*1001)
  {
    Serial.println( ntp_client.getFormattedTime() );
    last = millis();
  }
}

void ntp_wifi_down( )
{
  ntp_ticker.detach();
  ntp_client.end();
}

void ntp_refresh( int phase = 0 )
{
  Serial.println( F("refresh NTP") );

  int wait;
  if (ntp_client.update())
  {
    wait = 10 * 60; // ten minutes after success 
    phase = 0; // restart at 1 second on next fail
  }
  else
  {
    wait = 1 << phase; // 1, 2, 4,... seconds
    phase = min( ++phase, 5 ); // 1 << 5 == 32 seconds
  }

  ntp_ticker.once_scheduled( wait, [phase](){ ntp_refresh( phase ); } );
}

void ntp_wifi_up( )
{
  ntp_client.begin();
  ntp_refresh();
}

#endif

// ----------------------------------------------------------------------------

void wifi_disconnected( const WiFiEventStationModeDisconnected & )
{
  pattern = PATTERN_WIFI_DISCONNECTED;
  Serial.println( F("WiFi disconnected") );

  schedule_function( []() {
#ifdef NODE_HAS_MQTT
    mqtt_wifi_down();
#endif
#ifdef NODE_HAS_WEB
    web_wifi_down();
#endif
#ifdef NODE_HAS_NTP
    ntp_wifi_down();
#endif
  } );
}

void wifi_connected( const WiFiEventStationModeConnected & )
{
  pattern = PATTERN_WIFI_CONNECTED;
  Serial.println( F("WiFi connected") );
}

void wifi_got_ip( const WiFiEventStationModeGotIP &e )
{
  pattern = PATTERN_WIFI_GOT_IP;
  Serial.print( F("WiFi got IP: ") );
  Serial.println( e.ip );

  schedule_function( []() {
#ifdef NODE_HAS_MQTT
    mqtt_wifi_up();
#endif
#ifdef NODE_HAS_WEB
    web_wifi_up();
#endif
#ifdef NODE_HAS_NTP
    ntp_wifi_up();
#endif
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

void setup() 
{
  Serial.begin(57600);
  Serial.println("");
  Serial.println("");
  Serial.println(Version);

  ms_callback_setup();
  uptime_setup();
  patterns_setup();
  wifi_setup();
#ifdef NODE_HAS_NTP
  ntp_setup();
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

void loop() 
{
  uptime_loop();  // just reporting
  //wifi_loop();
#ifdef NODE_HAS_NTP
  ntp_loop();  // just reporting
#endif
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
