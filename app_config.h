#pragma once

// ----------------------------------------------------------------------------

#define NODE_HAS_PIXELS  // ~3 kHz cost
#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_WEB_UPDATE
//#define NODE_HAS_OTA_UPDATE
#define NODE_HAS_INPUTS

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
#define MQTT_SUB_TOPIC "simple/cmd"
#define MQTT_PUB_TOPIC "simple/status"

#define WEBSERVER_PORT 80

#define LOOP_RATE_CHECK_INTERVAL_MS 7000

// ----------------------------------------------------------------------------

void app_setup();
void app_loop();

#ifdef NODE_HAS_PIXELS
bool app_pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) >pixel );
#endif

#ifdef NODE_HAS_MQTT
void app_mqtt_message( const char* data, int len );
#endif
