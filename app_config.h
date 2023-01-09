#pragma once

#include "app_config_dev.h"
//#include "app_config_doorotron.h"
//#include "app_config_heating.h"
//#include "app_config_island.h"

#include "private_config.h" // not in repo...

// ----------------------------------------------------------------------------

#ifndef WIFI_SSID
#define WIFI_SSID PRIVATE_WIFI_SSID
#endif
#ifndef WIFI_PASSWD
#define WIFI_PASSWD PRIVATE_WIFI_PASSWD
#endif
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "simple_node"
#endif

#ifndef NTP_HOST
#define NTP_HOST "europe.pool.ntp.org"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST PRIVATE_MQTT_HOST
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_CLIENT
#define MQTT_CLIENT "simple_node"
#endif
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 60  // timeout set to 1.5x this value
#endif
#ifndef MQTT_SUB_TOPIC
#define MQTT_SUB_TOPIC "simple/cmd"
#endif
#ifndef MQTT_PUB_TOPIC
#define MQTT_PUB_TOPIC "simple/status"
#endif

#ifndef PATTERN_PIN
#define PATTERN_PIN LED_BUILTIN
#endif

#ifndef PATTERN_WIFI_DISCONNECTED
#define PATTERN_WIFI_DISCONNECTED 0xAAAAAAAA
#endif
#ifndef PATTERN_WIFI_CONNECTED
#define PATTERN_WIFI_CONNECTED    0xF0F0F0F0
#endif
#ifndef PATTERN_WIFI_GOT_IP
#define PATTERN_WIFI_GOT_IP       0xFFFFFFFE
#endif

#ifndef PIXELS_PIN
#define PIXELS_PIN 13 // 13: dev, island
#endif
#ifndef NUMPIXELS
#define NUMPIXELS 251 // deliberately stressful
#endif

#ifndef WEBSERVER_PORT
#define WEBSERVER_PORT 80
#endif

#ifndef CONFIG_FILENAME
#define CONFIG_FILENAME "/node_config.bin"
#endif

// ----------------------------------------------------------------------------

void app_setup();
void app_loop();

#ifdef NODE_HAS_PIXELS
bool app_pixels_update( uint16_t num_pixels, std::function< void(uint16_t n, uint32_t c) >pixel );
#endif
