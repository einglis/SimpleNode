#pragma once

#define NODE_HAS_PIXELS  // ~3 kHz cost
#define NODE_HAS_NTP
#define NODE_HAS_MQTT  // ~9 kHz cost
#define NODE_HAS_WEB
#define NODE_HAS_WEB_UPDATE
//#define NODE_HAS_OTA_UPDATE
//#define NODE_HAS_INPUTS

// ----------------------------------------------------------------------------

#define WIFI_HOSTNAME "island"

#define PATTERN_WIFI_DISCONNECTED 0xAAAAAAAA
#define PATTERN_WIFI_CONNECTED    0xF0F0F0F0
#define PATTERN_WIFI_GOT_IP       0x00000000

#define PIXELS_PIN 13 // 13: dev, island
#define NUMPIXELS 251 // deliberately stressful

#define MQTT_CLIENT "island_client"
#define MQTT_SUB_TOPIC "island/cmd"
#define MQTT_PUB_TOPIC "island/status"
